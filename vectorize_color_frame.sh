#!/bin/sh
NB_COLORS=6

BASENAME=`basename $1 .ppm`
WIDTH=`identify -ping -format "%w" $1`

range() {
   echo | awk "BEGIN{ from=$1; to=$2}"' {
      for(i=from; i<=to; i++) {
         printf("%d ",i)
      }
   }'
}

select_color() {
   echo $COLORMAP | awk "BEGIN{ nb=$1; entry=$2}"' {
       for(i=0; i<nb; i++) {
           if(i<entry) printf("-fill \"#ffffff\""); else printf("-fill \"#000001\"");
	   e=(nb-1-i)*3+1
           printf(" -opaque \"#%06x\"\n",$(e+2)+$(e+1)*256+$e*256*256)
       }
  }'
}

convert $1 -filter lanczos -resize 200% $BASENAME"_scaled.png"
pngquant --nofs --force  $NB_COLORS $BASENAME"_scaled.png" -o $BASENAME"_reduced.png"
COLORMAP=`convert $BASENAME"_reduced.png" -unique-colors -compress none ppm:- | sed -n '4p'`

for i in `range 0 $NB_COLORS`
do
   cmd="convert "$BASENAME"_reduced.png "`select_color $NB_COLORS $i`" "$BASENAME"_"$i"_isolated.png"
   eval $cmd
   cmd="convert "$BASENAME"_"$i"_isolated.png -fill \"#FFFFFF\" -opaque \"#FFFFFF\" -fill \"#000000\" -opaque \"#000001\" "$BASENAME"_"$i"_layer.png"
   eval $cmd
done

