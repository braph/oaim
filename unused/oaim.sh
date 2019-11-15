#!/bin/bash

FPS=60
SHOOT_DELAY_MIN=0
SHOOT_DELAY_MAX=0
SHOOT_TIME=0
CROSSHAIR_NUM=1
SHOOT_ON_WHITE=0
SHOOT_ON_GREEN=0

show_help() {
cat << EOF
Usage: [-f FPS] [-s shoot duration] [-d shoot delay] [-WG] [-X crosshair] [-C color]
 -f <FPS>         Set FPS
 -s <SHOOT TIME>  Set shoot duration in milli seconds
 -d <SHOOT DELAY> Set shoot delay in milli seconds (Format: ShootDelayMin-ShootDelayMax)
 -G               Shoot on green
 -W               Shoot on white
 -C <COLOR>       Shoot on custom color
    (Format: RedMin-RedMax,GreenMin-GreenMax,BlueMin-BlueMax e.g. 0-70,130-256,0-90)\
-X <NUM>          Select crosshair
EOF
}

while getopts "hf:d:s:WG" opt; do
   case $opt in
     'f') FPS=$OPTARG;;
     'd') IFS=- read SHOOT_DELAY_MIN SHOOT_DELAY_MAX <<<"$optarg";;
     's') SHOOT_TIME=$OPTARG;;
     'X') CROSSHAIR_NUM=$OPTARG;;
     'h') show_help; exit 0;;
     'W') SHOOT_ON_WHITE=1;;
     'G') SHOOT_ON_GREEN=1;;
     \?) echo "Invalid option: -$OPTARG" >&2
         show_help; exit 1; ;;
   esac
done

window_match=$(xwininfo -root -all | grep -F -i -e 'openarena' -e 'yuoa.' | head -1)
if [[ "$window_match" ]]; then
  echo "Using $window_match"
  GAME_WINDOW_ID=$(grep -Eo '0x[0-9]+ ' <<< "$window_match")
else
  echo "Falling back on root window"
  GAME_WINDOW_ID=0
fi

gcc -lX11 -lXtst \
  -DFPS=$FPS \
  -DSHOOT_DELAY_MIN=$SHOOT_DELAY_MIN \
  -DSHOOT_DELAY_MAX=$SHOOT_DELAY_MAX \
  -DCROSSHAIR_NUM=$CROSSHAIR_NUM \
  -DSHOOT_TIME=$SHOOT_TIME \
  -DSHOOT_ON_WHITE=$SHOOT_ON_WHITE \
  -DSHOOT_ON_GREEN=$SHOOT_ON_GREEN \
  -DGAME_WINDOW_ID=$GAME_WINDOW_ID \
  -Ofast \
  oaim.static.c -o oaim && exec ./oaim

#          if (6 != sscanf(optarg, "%hu-%hu,%hu-%hu,%hu-%hu", &cr.red[0], &cr.red[1],
#                &cr.green[0], &cr.green[1], &cr.blue[0], &cr.blue[1]))
#            errx(1, "Invalid custom color format: %s", optarg);
#          printf("- Shooting on custom color: %s\n", optarg);
#ADD_COLOR_RANGE:
#          for (unsigned i = 0; i <= 1; ++i) {
#            if (cr.red[i] > 255) errx(1, "Invalid value for red: %u", cr.red[i]);
#            if (cr.blue[i] > 255) errx(1, "Invalid value for blue: %u", cr.blue[i]);
#            if (cr.green[i] > 255) errx(1, "Invalid value for green: %u", cr.green[i]);
#            cr.red[i] *= 256;
#            cr.blue[i] *= 256;
#            cr.green[i] *= 256;
#          }
#          color_ranges = realloc(color_ranges, ++color_ranges_count);
#          color_ranges[color_ranges_count-1] = cr;
