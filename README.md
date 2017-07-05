This repository contains a code to reassemble RAID6 with 7 disks and one missing disk from a broken 3ware 9650SE-raid controller. 

How to use:

g++ -O3 raid6_7x7.cpp -o raid6_7x7.x -fopenmp

./raid6_7x7.x missing /home/robert/7.iso /home/robert/6.iso /home/robert/3.iso /home/robert/0.iso /home/robert/2.iso /home/robert/4.iso /backup/test.raw

