# rs_ply_generator

## How to build

* mkdir build
* cd build
* cmake ..
* make -j\<number-of-cores+1\> (for example, make -j4)

## How to execute

* ./rs_ply_generator -b="\<.bag filename\>" -s=\<Boolean value\> -d=\<Boolean value\>

For example,

* .rs_ply_generator -b="20201218_rs1.bag" -s=false -d=true
