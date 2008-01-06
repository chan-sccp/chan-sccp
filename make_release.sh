#!/bin/sh

RELEASEDATE=`date +%Y%m%d`
FILENAME=chan_sccp_$RELEASEDATE.tar
DIR=chan_sccp-$RELEASEDATE
mkdir -p ".release"/$DIR
mkdir -p ".release"/$DIR/conf

cp *.c ".release"/$DIR
cp *.h ".release"/$DIR
cp create_config.sh ".release"/$DIR
cp Makefile ".release"/$DIR
cp -f -R conf/*.* ".release"/$DIR/conf
#cp -f -R contrib ".release"/$DIR
rm -f ".release"/$DIR/config.h
sed -e "s/#define SCCP_VERSION .*/#define SCCP_VERSION \"$RELEASEDATE\"/g" -i ".release"/$DIR/chan_sccp.h


cd ".release"
tar -cf $FILENAME $DIR
gzip -f $FILENAME
rm -R $DIR