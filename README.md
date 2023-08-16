# fastq_trimmer
Description:<br>
A lightweight and high-performance C++ tool for hard-trimming *.fastq (and *.fastq.gz) reads.<br>
<br>
Compilation (as done with Ubuntu Linux):<br>
<code>g++ -std=c++11 fastq_trimmer.cpp -o fastq_trimmer -lz -pthread -O2</code><br>
<br>
Usage:<br>
<code>./fastq_trimmer  --in/-i INPUT_DIRECTORY --out/-o OUTPUT_DIRECTORY [--N3prime/-3] 3_PRIME_TRIM_VALUE [--N5prime/-5] 5_PRIME_TRIM_VALUE</code>
