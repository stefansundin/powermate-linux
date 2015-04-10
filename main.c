#include <stdio.h>
#include <linux/input.h>

int main(int argc, char *argv[]) {
  // if (argc < 3) {
  //   fprintf(stderr, "Usage: %s /dev/input/powermate 1\n", argv[0]);
  //   fprintf(stderr, "The number is the percentage to change the volume with each input event.\n");
  //   return 1;
  // }
  // char *dev = argv[1];
  // int p = atoi(argv[2]);

  char dev[] = "/dev/input/powermate";
  int p = 2;

  // Test device
  FILE *f = fopen(dev, "rb");
  if (f == NULL) {
    fprintf(stderr, "Could not open %s\n", dev);
    return 2;
  }
  fclose(f);

  // daemonize
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

  while (1) {
    do {
      f = fopen(dev, "rb");
      if (f == NULL) {
        fprintf(stderr, "Could not open %s\n", dev);
        fprintf(stderr, "Sleeping 1 second.\n");
        sleep(1);
      }
    } while (f == NULL);

    while (!feof(f) && !ferror(f)) {
      struct input_event ev;
      size_t n = fread(&ev, 1, sizeof(ev), f);
      // fprintf(stderr, "type: %d\n", ev.type);
      // fprintf(stderr, "code: %d\n", ev.code);
      // fprintf(stderr, "value: %d\n", ev.value);
      // fprintf(stderr, "\n");
      // fflush(stderr);

      if (ev.type == 2 && ev.code == 7) {
        char buf[100];
        if (ev.value == -1) {
          // counter clock-wise turn
          sprintf(buf, "amixer -D pulse sset Master %d%%-", p);
          system(buf);
        }
        else if (ev.value == 1) {
          // clock-wise turn
          sprintf(buf, "amixer -D pulse sset Master %d%%+", p);
          system(buf);
        }
      }
      if (ev.type == 1 && ev.code == 256) {
        if (ev.value == 1) {
          // knob depressed
          system("amixer -D pulse set Master toggle");
        }
        else if (ev.value == 0) {
          // knob released
        }
      }
    }
    fprintf(stderr, "Device disappeared.\n");
    fclose(f);
  }

  return 0;
}
