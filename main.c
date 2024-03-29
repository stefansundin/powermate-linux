#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/input.h>
#include <pulse/pulseaudio.h>
#include "tomlc99/toml.h"

// Some example code:
// https://sowerbutts.com/powermate/

// Settings
char *dev = "/dev/input/powermate";
char *sink_name = NULL;
short led_enabled = 1;
pa_volume_t p = 2.0*PA_VOLUME_NORM/100;
int min_volume = 0;
int max_volume = PA_VOLUME_NORM;
char *press_command = NULL;
char *long_press_command = NULL;
char *clock_wise_command = NULL;
char *counter_clock_wise_command = NULL;
char *press_clock_wise_command = NULL;
char *press_counter_clock_wise_command = NULL;
int64_t long_press_ms = 1000;

// State
short muted = 0;
short knob_depressed = 0;
short knob_depressed_rotated = 0;
int devfd = 0;
struct timeval knob_depressed_timestamp;

struct pollfd *pfds = NULL;
int pa_nfds = 0;
int sink_index = -1;
pa_context *context = NULL;
pa_cvolume vol;

void exec_command(char *command) {
  if (command == NULL || command[0] == '\0') {
    return;
  }
  printf("Executing: %s\n", command);
  int ret = system(command);
  if (ret != 0) {
    printf("Return value: %d\n", ret);
  }
}

void set_led(unsigned int val) {
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
  if (muted || !led_enabled) {
    set_led(0);
  } else {
    const pa_volume_t max_vol = pa_cvolume_max(&vol);
    unsigned int val = MIN(max_vol, PA_VOLUME_NORM);
    set_led(val * 255 / PA_VOLUME_NORM);
  }
}

// based on https://github.com/pulseaudio/pulseaudio/blob/v12.2/src/pulse/volume.c#L456-L468
int pa_cvolume_channels_equal(const pa_cvolume *a) {
  unsigned c;
  for (c = 1; c < a->channels; c++)
    if (a->values[c] != a->values[0])
      return 0;
  return 1;
}

// based on https://github.com/pulseaudio/pulseaudio/blob/v12.2/src/pulse/volume.c#L141-L153
pa_volume_t pa_cvolume_min_unmuted(const pa_cvolume *a) {
  pa_volume_t m = PA_VOLUME_MAX;
  unsigned c;
  for (c = 0; c < a->channels; c++)
    if (a->values[c] < m && a->values[c] != 0)
      m = a->values[c];
  return m;
}

void pa_sink_info_callback(pa_context *context, const pa_sink_info *info, int eol, void *userdata) {
  if (eol) {
    return;
  }
  if (sink_index == info->index && !memcmp(&vol,&info->volume,sizeof(vol)) && info->mute == muted) {
    return;
  }
  sink_index = info->index;
  memcpy(&vol, &info->volume, sizeof(vol));
  muted = info->mute;
  const pa_volume_t max_vol = pa_cvolume_max(&vol);
  printf("New volume (sink %d): %5d (%6.2f%%), muted: %d\n", sink_index, max_vol, max_vol*100.0/PA_VOLUME_NORM, muted);
  update_led();
}

void pa_server_info_callback(pa_context *c, const pa_server_info *info, void *userdata) {
  printf("Sink name: %s\n", info->default_sink_name);
  // get data about default sink
  pa_operation_unref(pa_context_get_sink_info_by_name(context, info->default_sink_name, pa_sink_info_callback, NULL));
}

void refresh_sink_info() {
  if (sink_name == NULL) {
    pa_operation_unref(pa_context_get_server_info(context, pa_server_info_callback, NULL));
  } else {
    pa_operation_unref(pa_context_get_sink_info_by_name(context, sink_name, pa_sink_info_callback, NULL));
  }
}

void pa_event_callback(pa_context *context, pa_subscription_event_type_t t, uint32_t index, void *userdata) {
  pa_subscription_event_type_t type = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
  // printf("new event: %04x (type: %04x, index: %d)\n", t, type, index);
  if (type == PA_SUBSCRIPTION_EVENT_SERVER) {
    // sink might have changed, refresh server info
    refresh_sink_info();
  } else if (type == PA_SUBSCRIPTION_EVENT_SINK && index == sink_index) {
    // volume change
    pa_operation_unref(pa_context_get_sink_info_by_index(context, sink_index, pa_sink_info_callback, NULL));
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
      fprintf(stderr, "Error: %s\n", strerror(errno));
      sleep(1);
    } else {
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

  // if the knob is depressed, then we need to timeout for the sake of detecting a long press
  if (knob_depressed && !knob_depressed_rotated && (long_press_command == NULL || long_press_command[0] != '\0')) {
    struct timeval now;
    if (gettimeofday(&now, NULL) < 0) {
      fprintf(stderr, "gettimeofday failed\n");
    }
    timeout = (long_press_ms+knob_depressed_timestamp.tv_sec*1000+knob_depressed_timestamp.tv_usec/1000) - (now.tv_sec*1000+now.tv_usec/1000);
    // fprintf(stderr, "timeout=%d\n", timeout);
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

  if (knob_depressed && !knob_depressed_rotated && ret == 0) {
    // timer ran out
    knob_depressed = 0;
    if (long_press_command == NULL) {
      led_enabled = !led_enabled;
      printf("LED enabled: %d\n", led_enabled);
    } else {
      exec_command(long_press_command);
    }
    update_led();
  }

  if (pfds[nfds].revents > 0) {
    // fprintf(stderr, "fd: %d. events: %d. revents: %d.\n", pfds[nfds].fd, pfds[nfds].events, pfds[nfds].revents);
    struct input_event ev;
    int n = read(devfd, &ev, sizeof(ev));
    if (n != sizeof(ev)) {
      printf("Device disappeared!\n");
      devfd = -1;
    } else {
      if (ev.type == EV_REL && ev.code == 7) {
        pa_cvolume v;
        memcpy(&v, &vol, sizeof(v));
        if (ev.value == -1) {
          // counter clockwise turn
          if (knob_depressed) {
            exec_command(press_counter_clock_wise_command);
            knob_depressed_rotated = 1;
          } else {
            if (counter_clock_wise_command == NULL) {
              if ((min_volume == 0 && (pa_cvolume_channels_equal(&v) || pa_cvolume_min_unmuted(&v) > p))
               || pa_cvolume_min_unmuted(&v) > min_volume) {
                // we can lower the volume and maintain the balance if:
                // 1. there is no inbalance (all channels have the same volume)
                // 2. min volume on unmuted channels is greater than min_volume
                pa_cvolume_dec(&v, MIN(p, pa_cvolume_min_unmuted(&v)-min_volume));
                pa_context_set_sink_volume_by_index(context, sink_index, &v, NULL, NULL);
              }
            } else {
              exec_command(counter_clock_wise_command);
            }
          }
        } else if (ev.value == 1) {
          // clockwise turn
          if (knob_depressed) {
            exec_command(press_clock_wise_command);
            knob_depressed_rotated = 1;
          } else {
            if (clock_wise_command == NULL) {
              int maxvol = max_volume;
              if (max_volume == PA_VOLUME_NORM && pa_cvolume_max(&v) > PA_VOLUME_NORM) {
                // we're already above 100%, so allow volume up to 150%
                // see "Allow louder than 100%" in sound settings
                maxvol *= 1.50;
              }
              pa_cvolume_inc_clamp(&v, p, maxvol);
              pa_context_set_sink_volume_by_index(context, sink_index, &v, NULL, NULL);
            } else {
              exec_command(clock_wise_command);
            }
          }
        }
      } else if (ev.type == EV_KEY && ev.code == 256) {
        if (ev.value == 1) {
          // knob depressed
          knob_depressed = 1;
          knob_depressed_timestamp = ev.time;
        } else if (ev.value == 0 && knob_depressed) {
          // knob released
          knob_depressed = 0;
          if (!knob_depressed_rotated) {
            if (press_command == NULL) {
              pa_context_set_sink_mute_by_index(context, sink_index, !muted, NULL, NULL);
            } else {
              exec_command(press_command);
            }
          }
          knob_depressed_rotated = 0;
        }
      }
    }
  }

  // copy back pulseaudio revents
  memcpy(ufds, pfds, nfds*sizeof(struct pollfd));

  return ret;
}

char *get_config_home() {
  char *xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home != NULL && xdg_config_home[0] != '\0') {
    return xdg_config_home;
  }
  char *homedir = getenv("HOME");
  if (homedir == NULL) {
    return NULL;
  }
  char config_home[PATH_MAX] = "";
  snprintf(config_home, sizeof(config_home), "%s/.config", homedir);
  if (setenv("XDG_CONFIG_HOME", config_home, 1) != 0) {
    return NULL;
  }
  return getenv("XDG_CONFIG_HOME");
}

int main(int argc, char *argv[]) {
  int i;
  for (i=1; i < argc; i++) {
    if ((strcmp(argv[i],"-c") && strcmp(argv[i],"-d"))     // check if it's an unexpected option
     || (!strcmp(argv[i],"-c") && ++i == argc)             // check if it's -c but the filename is missing
    ) {
      fprintf(stderr, "Usage: %s [-c file] [-d]\n", argv[0]);
      return 0;
    }
  }

  // Settings
  int daemonize = 0;

  // Load config file
  {
    char config_path[PATH_MAX] = "";
    for (i=1; i < argc; i++) {
      if (!strcmp(argv[i], "-c")) {
        strcpy(config_path, argv[++i]);
        if (access(config_path, R_OK) != 0) {
          fprintf(stderr, "Could not access %s: %s\n", config_path, strerror(errno));
          return 1;
        }
      }
    }

    char *config_home = get_config_home();
    if (config_path[0] == '\0' && config_home != NULL) {
      snprintf(config_path, sizeof(config_path), "%s/powermate.toml", config_home);
      if (access(config_path, R_OK) != 0) {
        config_path[0] = '\0';
      }
    }
    if (config_path[0] == '\0') {
      strcpy(config_path, "/etc/powermate.toml");
    }

    if (access(config_path, R_OK) == 0) {
      printf("Loading config from %s\n", config_path);

      FILE *f;
      if ((f = fopen(config_path, "r")) == NULL) {
        fprintf(stderr, "Failed to open file.\n");
      } else {
        char errbuf[200];
        toml_table_t *conf = toml_parse_file(f, errbuf, sizeof(errbuf));
        fclose(f);
        if (conf == 0) {
          fprintf(stderr, "Error: %s\n", errbuf);
        } else {
          toml_datum_t ret;
          if ((ret=toml_string_in(conf,"dev")).ok) {
            dev = ret.u.s;
          } else if (toml_key_exists(conf,"dev")) {
            fprintf(stderr, "Warning: bad value in 'dev', expected a string.\n");
          }
          if ((ret=toml_string_in(conf,"sink_name")).ok) {
            sink_name = ret.u.s;
          } else if (toml_key_exists(conf,"sink_name")) {
            fprintf(stderr, "Warning: bad value in 'sink_name', expected a string.\n");
          }
          if ((ret=toml_bool_in(conf,"daemonize")).ok) {
            daemonize = ret.u.b;
          } else if (toml_key_exists(conf,"daemonize")) {
            fprintf(stderr, "Warning: bad value in 'daemonize', expected a boolean.\n");
          }
          if ((ret=toml_bool_in(conf,"led_enabled")).ok) {
            led_enabled = ret.u.b;
          } else if (toml_key_exists(conf,"led_enabled")) {
            fprintf(stderr, "Warning: bad value in 'led_enabled', expected a boolean.\n");
          }
          if ((ret=toml_double_in(conf,"p")).ok) {
            p = ret.u.d*PA_VOLUME_NORM/100;
          } else if (toml_key_exists(conf,"p")) {
            fprintf(stderr, "Warning: bad value in 'p', expected a double.\n");
          }
          if ((ret=toml_double_in(conf,"min_volume")).ok) {
            min_volume = ret.u.d*PA_VOLUME_NORM/100;
          } else if (toml_key_exists(conf,"min_volume")) {
            fprintf(stderr, "Warning: bad value in 'min_volume', expected a double.\n");
          }
          if ((ret=toml_double_in(conf,"max_volume")).ok) {
            max_volume = ret.u.d*PA_VOLUME_NORM/100;
          } else if (toml_key_exists(conf,"max_volume")) {
            fprintf(stderr, "Warning: bad value in 'max_volume', expected a double.\n");
          }
          if ((ret=toml_string_in(conf,"press_command")).ok) {
            press_command = ret.u.s;
          } else if (toml_key_exists(conf,"press_command")) {
            fprintf(stderr, "Warning: bad value in 'press_command', expected a string.\n");
          }
          if ((ret=toml_string_in(conf,"long_press_command")).ok) {
            long_press_command = ret.u.s;
          } else if (toml_key_exists(conf,"long_press_command")) {
            fprintf(stderr, "Warning: bad value in 'long_press_command', expected a string.\n");
          }
          if ((ret=toml_string_in(conf,"clock_wise_command")).ok) {
            clock_wise_command = ret.u.s;
          } else if (toml_key_exists(conf,"clock_wise_command")) {
            fprintf(stderr, "Warning: bad value in 'clock_wise_command', expected a string.\n");
          }
          if ((ret=toml_string_in(conf,"counter_clock_wise_command")).ok) {
            counter_clock_wise_command = ret.u.s;
          } else if (toml_key_exists(conf,"counter_clock_wise_command")) {
            fprintf(stderr, "Warning: bad value in 'counter_clock_wise_command', expected a string.\n");
          }
          if ((ret=toml_string_in(conf,"press_clock_wise_command")).ok) {
            press_clock_wise_command = ret.u.s;
          } else if (toml_key_exists(conf,"press_clock_wise_command")) {
            fprintf(stderr, "Warning: bad value in 'press_clock_wise_command', expected a string.\n");
          }
          if ((ret=toml_string_in(conf,"press_counter_clock_wise_command")).ok) {
            press_counter_clock_wise_command = ret.u.s;
          } else if (toml_key_exists(conf,"press_counter_clock_wise_command")) {
            fprintf(stderr, "Warning: bad value in 'press_counter_clock_wise_command', expected a string.\n");
          }
          if ((ret=toml_int_in(conf,"long_press_ms")).ok) {
            long_press_ms = ret.u.i;
          } else if (toml_key_exists(conf,"long_press_ms")) {
            fprintf(stderr, "Warning: bad value in 'long_press_ms', expected an integer.\n");
          }
        }
        toml_free(conf);
      }
    } else {
      printf("Config file not found, using defaults. Checked the following paths:\n");
      if (config_home != NULL) {
        printf("- %s/powermate.toml\n", config_home);
      }
      printf("- /etc/powermate.toml\n");
      printf("\n");
    }
  }
  if (sink_name != NULL) {
    printf("Sink name: %s\n", sink_name);
  }

  for (i=1; i < argc; i++) {
    if (!strcmp(argv[i], "-d")) {
      daemonize = 1;
    }
  }

  // Test device
  devfd = open(dev, O_RDWR);
  if (devfd == -1) {
    fprintf(stderr, "Could not open %s: %s\n", dev, strerror(errno));
    fprintf(stderr, "Don't worry, it will be opened automatically if it appears.\n");
    fprintf(stderr, "If you just installed this program, you might have to unplug the device and then plug it back in..\n");
  }

  // Daemonize
  if (daemonize) {
    int pid = fork();
    if (pid == 0) {
      // We're the child process!
      // Release handle to the working directory
      if (chdir("/") < 0) {
        fprintf(stderr, "chdir() failed");
      }
      // Close things
      fclose(stdin);
      fclose(stdout);
      fclose(stderr);
    } else if (pid < 0) {
      fprintf(stderr, "Failed to become a daemon.\n");
    } else {
      printf("Just became a daemon.\n");
      return 0;
    }
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

    // We're connected, get sink data
    refresh_sink_info();

    // Subscribe to pulseaudio events
    pa_context_set_subscribe_callback(context, pa_event_callback, NULL);
    pa_operation *o = pa_context_subscribe(context, PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SERVER, NULL, NULL);
    if (o == NULL) {
      fprintf(stderr, "pa_context_subscribe() failed");
      return 1;
    }
    pa_operation_unref(o);

    // Set up our custom poll function
    pa_mainloop_set_poll_func(mainloop, poll_func, NULL);

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
