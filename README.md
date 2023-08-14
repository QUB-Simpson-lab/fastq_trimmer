# fastq_trimmer
Description:<br>
A lightweight and high-performance C++ tool for hard-trimming *.fastq (and *.fastq.gz) reads.<br>
<br>
Compilation (as done with Ubuntu Linux):<br>
<code>g++ -std=c++11 fastq_trimmer.cpp -o fastq_trimmer -lz -pthread -O2</code><br>
<br>
Usage:<br>
<code>./fastq_trimmer --in INPUT_DIRECTORY --out OUTPUT_DIRECTORY --N TRIM_VALUE</code>
