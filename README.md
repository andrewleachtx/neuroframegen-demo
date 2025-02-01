# neuroframegen-demo

## Building
```sh
git submodule init
git submodule update

conda env create -f environment.yml
conda activate gcc-env

cmake -S . -B build

chmod +x build.sh
./build.sh
```