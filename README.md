# rs_ply_generator

## How to build

* mkdir build
* cd build
* cmake ..
* make -j\<number-of-cores+1\> (for example, make -j4)

## How to execute

* ./rs_ply_generator -b="\<.bag filename\>" -s=\<Boolean value\> -d=\<Boolean value\>

For example,

* ./rs_ply_generator -b="20201218_rs1.bag" -s=false -d=true


## Mandatory change

* move "segnet" folder inside "build" folder
* inside "build" folder make two new directory: "inference" and "mask"
* a Python environment with this requirement

tensorflow_gpu==2.3.0
numpy==1.16.4
Keras==2.4.3
opencv_python_headless==4.4.0.42
Pillow==7.2.0
tensorflow==2.3.1 
