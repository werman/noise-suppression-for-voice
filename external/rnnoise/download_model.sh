#!/bin/sh
set -e

version=`cat model_version`
model=rnnoise_data-$version.tar.gz

if [ ! -f $model ]; then
        echo "Downloading latest model"
        wget https://media.xiph.org/rnnoise/models/$model
fi
tar xvomf $model

