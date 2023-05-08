
# Vectorizer: converts a black and white video into triangulations (WIP)
# Bruno Levy, April 2023
# License: BSD 3 clauses

VIDEOSOURCE=https://ia802905.us.archive.org/19/items/TouhouBadApple/Touhou%20-%20Bad%20Apple.mp4
INPUT_VIDEOFILE=VIDEO/video.mp4
#FPS=5
#RESOLUTION=256

FPS=12
RESOLUTION=128
TOLERANCE=20.0
NB_COLORS=2

####################################################################

# fig2obj: converts an xfig file with paths into an alias|wavefront file
# usage: fig2obj input.fig output.obj
fig2obj() {
   awk < $1 '
      BEGIN {
         v_offset=1;
         nb_in_polyline=0;
         nb_remaining=0;
      } {
         if($1 == 3 && NF== 14) {
           nb_in_polyline = $14
           nb_remaining = $14
         } else if(nb_remaining != 0) {
           printf("v %d %d\n", $1, -$2)
           --nb_remaining
           if(nb_remaining == 0) {
              for(i=0; i<nb_in_polyline; ++i) {
                 printf("l %d %d\n", v_offset+i, v_offset+((i+1)%nb_in_polyline))
              }
              v_offset += nb_in_polyline
           }
         }
      }
   '
} > $2

####################################################################

fig2movetolineto() {
   awk < $1 '
      BEGIN {
         v_offset=1;
         nb_in_polyline=0;
         nb_remaining=0;
      } {

        if ($1 == 6 && NF == 5)
        {
          minx = $2
          miny = $3
          maxx = $4
          maxy = $5

          dx = maxx - minx
          dy = maxy - miny
        }
        else

         if($1 == 3 && NF == 14) {
           nb_in_polyline = $14
           nb_remaining = $14
         } else if(nb_remaining != 0) {
           if (nb_in_polyline == nb_remaining)
           {
             startx = $1
             starty = $2
             printf("moveto %d %d\n", $1-minx, dy - ($2-miny))
           }
           else printf("lineto %d %d\n", $1-minx, dy - ($2-miny))

           --nb_remaining
           if(nb_remaining == 0) printf("lineto %d %d\n\n", startx-minx, dy - (starty-miny))
         }
      }
   '
} > $2

####################################################################

vectorize_BW() {
   BASENAME=`basename $1 .pgm`
   FIGFRAME=PATHS/$BASENAME.fig
   OBJFRAME=PATHS/$BASENAME.obj
   VECTORFRAME=PATHS/$BASENAME.vec
   potrace -b xfig -a 0 -O $TOLERANCE -r $RESOLUTION"x"$RESOLUTION $1 -o $FIGFRAME
   fig2obj $FIGFRAME $OBJFRAME
   fig2movetolineto $FIGFRAME $VECTORFRAME
}
    
####################################################################

# range from to
# prints the range of numbers from from+1 ... to
range() {
   echo | awk "BEGIN{ from=$1; to=$2}"' {
      for(i=from; i<=to; i++) {
         printf("%d ",i)
      }
   }'
}

# select_color colormap nb_colors entry
# prints the imagemagic command option to select a given color in an image
select_color() {
   echo "$COLORMAP" | awk "BEGIN{ nb=$1; entry=$2}"' {
       for(i=0; i<nb; i++) {
           if(i<entry) printf("-fill \"#ffffff\""); else printf("-fill \"#000001\"");
	   e=(nb-1-i)*3+1
           printf(" -opaque \"#%06x\"\n",$(e+2)+$(e+1)*256+$e*256*256)
       }
  }'
}

vectorize_color() {
    BASENAME=`basename $1 .ppm`
    BASEFRAME=FRAMES/$BASENAME
    convert $1 -filter lanczos -resize 200% $BASEFRAME"_scaled.png"
    pngquant --nofs --force  $NB_COLORS $BASEFRAME"_scaled.png" -o $BASEFRAME"_reduced.png"
    COLORMAP=`convert $BASEFRAME"_reduced.png" -unique-colors -compress none ppm:- | sed -n '4p'`
    for i in `range 0 $NB_COLORS`
    do
        cmd="convert "$BASEFRAME"_reduced.png "`select_color $NB_COLORS $i`" "$BASEFRAME"_"$i"_isolated.png"
        eval $cmd
        cmd="convert "$BASEFRAME"_"$i"_isolated.png -fill \"#FFFFFF\" -opaque \"#FFFFFF\" -fill \"#000000\" -opaque \"#000001\" "$BASEFRAME"_"$i"_layer.ppm"
        eval $cmd
        potrace -b xfig -a 0 -O $TOLERANCE -r $RESOLUTION"x"$RESOLUTION $BASEFRAME"_"$i"_layer.ppm" -o PATHS/$BASENAME"_"$i"_layer".fig
        fig2obj PATHS/$BASENAME"_"$i"_layer".fig PATHS/$BASENAME"_"$i"_layer".obj
    done
}

####################################################################

while [ -n "$1" ]; do
    case "$1" in
        -fps)
            shift
            FPS=$1
            shift
            ;;
        -r | -resolution )
            shift
            RESOLUTION=$1
            shift
            ;;
        -nc | -nb_colors)
            shift
            NB_COLORS=$1
            shift
            ;;
        -O | -tolerance)
            shift
            TOLERANCE=$1
            shift
            ;;
        -i | -input)
            shift
            INPUT_VIDEOFILE=$1
            shift
            ;;
        -h | -help | --help)
            cat <<EOF
NAME
    vectorize.sh

SYNOPSIS
    vectorizes a video

USAGE
    vectorize.sh [options] 

OPTIONS

    -h,-help,--help
        Prints this page.

    -fps nnn
        Frames per second. Default=12

    -r,-resolution  nnn
        Internal resolution for vectorizing. Default=128
    
    -nc,-nb_colors nnn
        Number of colors. Default=2 (black and white)

    -O,-tolerance
        5.0 and above for allowing to simplify the result.
	Default is 20.0. potrace's default is 0.2

    -i,-input videofile.mp4        
EOF
            exit
            ;;
        *)
            echo "Error: unrecognized option: $1"
            exit
            ;;
    esac
done

####################################################################

mkdir -p VIDEO FRAMES PATHS

# Download video
#echo "$0: [step 0] downloading video..."
#if [ ! -f VIDEO/video.mp4 ]; then
#  wget $VIDEOSOURCE -O VIDEO/video.mp4
#else
#  echo "   Using cached VIDEO/video.mp4"
#fi

# Step 1: extract frames
echo "$0: [step 1] extracting frames..."
rm -f FRAMES/*
if [[ "$NB_COLORS" -lt 3 ]]; then
   ffmpeg -i $INPUT_VIDEOFILE -vf fps=$FPS,scale=$RESOLUTION:-2,setsar=1:1 FRAMES/frame%04d.pgm
else
   ffmpeg -i $INPUT_VIDEOFILE -vf fps=$FPS,scale=$RESOLUTION:-2,setsar=1:1 FRAMES/frame%04d.ppm
fi

# Step 2: trace frames
echo "$0: [step 2] tracing frames..."
rm -f PATHS/*
if [[ "$NB_COLORS" -lt 3 ]]; then
    for frame in `ls FRAMES/*.pgm`
    do
        vectorize_BW $frame        
    done
else
    for frame in `ls FRAMES/*.ppm`
    do
        vectorize_color $frame        
    done
fi

