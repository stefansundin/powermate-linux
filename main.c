#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pulse/pulseaudio.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// Some example code:
// http://sowerbutts.com/powermate/

char dev[] = "/dev/input/powermate";
int p = 2;

char *sink_name = NULL;
pa_cvolume vol;
int muted;

void pa_server_info_callback(pa_context *c, const pa_server_info *info, void *userdata) {
  int len = strlen(info->default_sink_name)+1;
  sink_name = malloc(len);
  memcpy(sink_name, info->default_sink_name, len);
  printf("sink_name: %s\n", sink_name);
}

void pa_sink_info_callback(pa_context* context, const pa_sink_info* info, int eol, void* userdata) {
  if (eol) {
    return;
  }

  memcpy(&vol, &info->volume, sizeof(vol));
  muted = info->mute;
  printf("new volume: %d (%.2f%%), muted: %d\n", info->volume.values[0], info->volume.values[0]*100.0/PA_VOLUME_NORM, info->mute);

  // update LED
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = EV_MSC;
  ev.code = MSC_PULSELED;
  if (!muted) {
    ev.value = vol.values[0] * 255 / PA_VOLUME_NORM;
  }
  printf("set led: %d\n", ev.value);
  // write
  int fd = open(dev, O_WRONLY);
  if (fd == -1) {
    fprintf(stderr, "Could not open %s: %s\n", dev, strerror(errno));
    return;
  }
  if (write(fd, &ev, sizeof(ev)) != sizeof(ev)) {
    fprintf(stderr, "write(): %s\n", strerror(errno));
  }
  close(fd);
}

void pa_event_callback(pa_context *context, pa_subscription_event_type_t t, uint32_t index, void *userdata) {
  if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
    pa_operation *o = pa_context_get_sink_info_by_name(context, sink_name, pa_sink_info_callback, NULL);
    pa_operation_unref(o);
  }
}

void pa_state_callback(pa_context* context, void* ptr) {
  pa_operation *o;
  pa_threaded_mainloop *mainloop = (pa_threaded_mainloop*) ptr;
  pa_context_state_t state = pa_context_get_state(context);
  if (state == PA_CONTEXT_READY) {
    o = pa_context_get_server_info(context, pa_server_info_callback, NULL);
    pa_operation_unref(o);

    o = pa_context_get_sink_info_by_name(context, sink_name, pa_sink_info_callback, NULL);
    pa_operation_unref(o);

    pa_context_set_subscribe_callback(context, pa_event_callback, NULL);
    o = pa_context_subscribe(context, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);
    if (o == NULL) {
      fprintf(stderr, "pa_context_subscribe() failed");
      return;
    }
    pa_operation_unref(o);
  }
}

int main(int argc, char *argv[]) {
  // Test device
  int fd = open(dev, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Could not open %s: %s\n", dev, strerror(errno));
    return 1;
  }

  // Daemonize
  int pid = fork();
  if (pid == 0) {
    // We're the child process!
    // Release handle to working directory
    chdir("/");
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

  // PulseAudio
  pa_threaded_mainloop *mainloop = pa_threaded_mainloop_new();
  pa_threaded_mainloop_start(mainloop);
  pa_threaded_mainloop_lock(mainloop);
  // Get a context
  pa_context *context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "powermate");
  if (context == NULL) {
    fprintf(stderr, "pa_context_new failed\n");
    return 1;
  }
  pa_context_set_state_callback(context, pa_state_callback, NULL);
  // Connect
  if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
    fprintf(stderr, "pa_context_connect failed\n");
    pa_threaded_mainloop_unlock(mainloop);
    return 1;
  }
  pa_threaded_mainloop_unlock(mainloop);

  while (1) {
    while (fd == -1) {
      fd = open(dev, O_RDONLY);
      if (fd == -1) {
        fprintf(stderr, "Could not open %s: %s\n", dev, strerror(errno));
        fprintf(stderr, "Sleeping 1 second.\n");
        sleep(1);
      }
    }

    int n;
    while (1) {
      struct input_event ev;
      int n = read(fd, &ev, sizeof(ev));
      if (n != sizeof(ev)) {
        break;
      }

      if (ev.type == EV_REL && ev.code == 7) {
        pa_volume_t newvol;
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
        // char buf[100];
        // pa_cvolume_snprint(buf, sizeof(buf), &vol);
        // printf("set volume: %d (%.2f%%)\n", vol.values[0], vol.values[0]*100.0/PA_VOLUME_NORM);
        pa_threaded_mainloop_lock(mainloop);
        pa_context_set_sink_volume_by_name(context, sink_name, &vol, NULL, NULL);
        pa_threaded_mainloop_unlock(mainloop);
      }
      else if (ev.type == EV_KEY && ev.code == 256) {
        if (ev.value == 1) {
          // knob depressed
          printf("set mute: %d\n", !muted);
          pa_threaded_mainloop_lock(mainloop);
          pa_context_set_sink_mute_by_name(context, sink_name, !muted, NULL, NULL);
          pa_threaded_mainloop_unlock(mainloop);
        }
        else if (ev.value == 0) {
          // knob released
        }
      }
    }
    fprintf(stderr, "Device disappeared.\n");
    close(fd);
    fd = -1;
  }

  return 0;
}
