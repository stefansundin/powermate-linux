#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/input.h>
#include <pulse/pulseaudio.h>

// Some example code:
// http://sowerbutts.com/powermate/

char dev[] = "/dev/input/powermate";
int p = 2;
int movie_mode_timeout = 1000; // milliseconds

int devfd = 0;
short knob_depressed = 0;
struct timeval knob_depressed_timestamp;

struct pollfd *pfds = NULL;
int pa_nfds = 0;
char *sink_name = NULL;
pa_context *context = NULL;
pa_cvolume vol;
short muted = 0;
short movie_mode = 0;

void set_led(val) {
  // printf("set_led(%d)\n", val);
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = EV_MSC;
  ev.code = MSC_PULSELED;
  ev.value = val;
  if (write(devfd, &ev, sizeof(ev)) != sizeof(ev)) {
    fprintf(stderr, "write(): %s\n", strerror(errno));
  }
}

void update_led() {
  if (muted || movie_mode) {
    set_led(0);
  }
  else {
    set_led(vol.values[0] * 255 / PA_VOLUME_NORM);
  }
}

void pa_server_info_callback(pa_context *c, const pa_server_info *info, void *userdata) {
  int len = strlen(info->default_sink_name)+1;
  sink_name = malloc(len);
  memcpy(sink_name, info->default_sink_name, len);
  printf("Sink name: %s\n", sink_name);
}

void pa_sink_info_callback(pa_context* context, const pa_sink_info* info, int eol, void* userdata) {
  if (eol) {
    return;
  }
  printf("New volume: %5d (%6.2f%%), muted: %d\n", info->volume.values[0], info->volume.values[0]*100.0/PA_VOLUME_NORM, info->mute);

  memcpy(&vol, &info->volume, sizeof(vol));
  muted = info->mute;
  update_led();
}

void pa_event_callback(pa_context *context, pa_subscription_event_type_t t, uint32_t index, void *userdata) {
  if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
    pa_operation_unref(pa_context_get_sink_info_by_name(context, sink_name, pa_sink_info_callback, NULL));
  }
}

int poll_func(struct pollfd *ufds, unsigned long nfds, int timeout, void *userdata) {
  // alloc array that fits ufds and devfd
  if (nfds > pa_nfds) {
    pfds = realloc(pfds, (nfds+1)*sizeof(struct pollfd));
    if (pfds == NULL) {
      fprintf(stderr, "realloc failed\n");
      exit(1);
    }
    pa_nfds = nfds;
  }
  memcpy(pfds, ufds, nfds*sizeof(struct pollfd));

  // wait for devfd
  while (devfd < 0) {
    fprintf(stderr, "Attempting to open %s\n", dev);
    devfd = open(dev, O_RDWR);
    if (devfd == -1) {
      fprintf(stderr, "Could not open %s: %s\n", dev, strerror(errno));
      sleep(1);
    }
    else {
      printf("Device connected!\n");
      // When the device is connected, the kernel driver sets the LED to 50% brightness
      // We have to update the LED to represent the current volume
      update_led();
    }
  }

  // add devfd
  pfds[nfds].fd = devfd;
  pfds[nfds].events = POLLIN;
  pfds[nfds].revents = 0;

  // if knob is depressed, we need to timeout for the sake of detecting movie mode
  if (knob_depressed) {
    struct timeval now;
    if (gettimeofday(&now, NULL) < 0) {
      fprintf(stderr, "gettimeofday failed\n");
    }
    timeout = (movie_mode_timeout+knob_depressed_timestamp.tv_sec*1000+knob_depressed_timestamp.tv_usec/1000) - (now.tv_sec*1000+now.tv_usec/1000);
    fprintf(stderr, "timeout=%d\n", timeout);
  }

  int ret = poll(pfds, nfds+1, timeout);
  if (ret < 0) {
    fprintf(stderr, "poll failed\n");
    exit(1);
  }

  // int i;
  // for (i=0; i < nfds+1; i++) {
  //   fprintf(stderr, "%d: fd: %d. events: %d. revents: %d.\n", i, pfds[i].fd, pfds[i].events, pfds[i].revents);
  // }

  if (knob_depressed && ret == 0) {
    // timer ran out
    // fprintf(stderr, "knob depressed for %d milliseconds!\n", movie_mode_timeout);
    knob_depressed = 0;
    movie_mode = !movie_mode;
    printf("Movie mode: %d\n", movie_mode);
    update_led();
    // if muted, unmute
    if (muted) {
      pa_context_set_sink_mute_by_name(context, sink_name, !muted, NULL, NULL);
    }
  }

  if (pfds[nfds].revents > 0) {
    // fprintf(stderr, "fd: %d. events: %d. revents: %d.\n", pfds[nfds].fd, pfds[nfds].events, pfds[nfds].revents);
    struct input_event ev;
    int n = read(devfd, &ev, sizeof(ev));
    if (n != sizeof(ev)) {
      printf("Device disappeared!\n");
      devfd = -1;
    }
    else {
      if (ev.type == EV_REL && ev.code == 7) {
        pa_volume_t newvol = vol.values[0];
        if (ev.value == -1) {
          // counter clock-wise turn
          newvol = vol.values[0] - PA_VOLUME_NORM*p/100;
          if (newvol > vol.values[0]) {
            // we wrapped around, clamp to 0
            newvol = 0;
          }
        }
        else if (ev.value == 1) {
          // clock-wise turn
          newvol = MIN(vol.values[0] + PA_VOLUME_NORM*p/100, PA_VOLUME_NORM);
        }
        // set new volume
        pa_cvolume_set(&vol, vol.channels, newvol);
        pa_context_set_sink_volume_by_name(context, sink_name, &vol, NULL, NULL);
      }
      else if (ev.type == EV_KEY && ev.code == 256) {
        if (ev.value == 1) {
          // knob depressed
          knob_depressed = 1;
          knob_depressed_timestamp = ev.time;
          // printf("set mute: %d\n", !muted);
          pa_context_set_sink_mute_by_name(context, sink_name, !muted, NULL, NULL);
        }
        else if (ev.value == 0) {
          // knob released
          knob_depressed = 0;
        }
      }
    }
  }

  // copy back pulseaudio revents
  memcpy(ufds, pfds, nfds*sizeof(struct pollfd));

  return ret;
}

int main(int argc, char *argv[]) {
  // Test device
  devfd = open(dev, O_RDWR);
  if (devfd == -1) {
    fprintf(stderr, "Could not open %s: %s\n", dev, strerror(errno));
    fprintf(stderr, "Don't worry, it will be opened automatically if it appears.\n");
    fprintf(stderr, "If you just installed this program, you might have to unplug the device and then plug it back in..\n");
  }

  // Daemonize
  int pid = fork();
  if (pid == 0) {
    // We're the child process!
    // Release handle to working directory
    if (chdir("/") < 0) {
      fprintf(stderr, "chdir() failed");
    }
    // Close things
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
  }
  else if (pid < 0) {
    fprintf(stderr, "Failed to become a daemon, whatevs.\n");
  }
  else {
    printf("Just became a daemon, deal with it!\n");
    return 0;
  }

  while (1) {
    // PulseAudio
    pa_mainloop *mainloop = pa_mainloop_new();

    // Get a context
    context = pa_context_new(pa_mainloop_get_api(mainloop), "powermate");
    if (context == NULL) {
      fprintf(stderr, "pa_context_new failed\n");
      return 1;
    }

    // Connect
    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFAIL|PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
      fprintf(stderr, "pa_context_connect failed\n");
      return 1;
    }

    // Wait for connection to be ready
    pa_context_state_t state;
    do {
      if (pa_mainloop_iterate(mainloop, 1, NULL) < 0) {
        fprintf(stderr, "pa_mainloop_iterate failed\n");
        return 1;
      }
      state = pa_context_get_state(context);
    } while (state != PA_CONTEXT_READY);

    // We're connected, get sink and volume info
    pa_operation_unref(pa_context_get_server_info(context, pa_server_info_callback, NULL));
    pa_operation_unref(pa_context_get_sink_info_by_name(context, sink_name, pa_sink_info_callback, NULL));

    // Subscribe to new volume events
    pa_context_set_subscribe_callback(context, pa_event_callback, NULL);
    pa_operation *o = pa_context_subscribe(context, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);
    if (o == NULL) {
      fprintf(stderr, "pa_context_subscribe() failed");
      return 1;
    }
    pa_operation_unref(o);

    // Set up our custom poll function
    pa_mainloop_set_poll_func(mainloop, poll_func, NULL);

    // pa_context_state_t last_state = state;
    while (1) {
      if (pa_mainloop_iterate(mainloop, 1, NULL) < 0) {
        fprintf(stderr, "pa_mainloop_iterate failed\n");
        break;
      }
      state = pa_context_get_state(context);
      if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
        printf("PulseAudio connection lost!\n");
        // For some reason spawning pulseaudio after forking does not seem to work well, and if we try to reconnect too soon, it doesn't work. So just chill for 10 seconds.
        sleep(10);
        break;
      }
    }

    pa_context_disconnect(context);
    pa_mainloop_free(mainloop);
  }

  return 0;
}
