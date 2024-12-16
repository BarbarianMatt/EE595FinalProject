# Setup Instructions
Follow the instructions below to get a WSL with ns3, ns3 ai, and the correct conda virtual environment installed.

wsl --install -d Ubuntu

sudo apt update && sudo apt upgrade -y

sudo apt install git g++ build-essential gcc gdb python3 python3-pip -y

sudo apt install gcc-11 g++-11

echo 'export CC=gcc-11' >> ~/.bashrc
echo 'export CXX=g++-11' >> ~/.bashrc
source ~/.bashrc

cd /mnt/c/your-directory

git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev/contrib/
git clone https://github.com/Mauriyin/uwee595.git
cd ..

sudo apt install -y cmake

(confirms Install)
cmake --version (3.28.3)

git checkout -f f4cbb3a74 && git clean -fd
git apply contrib/uwee595/labdcf.patch

./ns3 configure --enable-examples
./ns3 build

(ai)
sudo apt install libboost-all-dev
sudo apt install libprotobuf-dev protobuf-compiler
sudo apt install pybind11-dev

git clone https://github.com/hust-diangroup/ns3-ai.git contrib/ai

mkdir -p ~/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
rm ~/miniconda3/miniconda.sh

source ~/miniconda3/bin/activate && conda deactivate

conda create -n ns3ai_env python=3.9

conda activate ns3ai_env

delete all examples other than a-plus-b in ai example, for both their folders and in CMakeLists.txt

./ns3 configure --enable-examples
./ns3 build ai

pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py

./ns3 build ns3ai_apb_msg_stru

(test with)
cd ./contrib/ai/examples/a-plus-b/use-msg-stru
python apb.py
cd ../../../../../

conda install jupyter ipykernel numpy scipy matplotlib

python -m ipykernel install --user --name=project_env --display-name "Python (project_env)"

(jupyter notebook)
install python extension
install jupyter extension

# if you get errors like, this is because 
r: no matching function for call to ‘NanoSeconds(ns3::Time)’
(this may fix the protobuf bug)
conda uninstall libprotobuf
pip uninstall protobuf

