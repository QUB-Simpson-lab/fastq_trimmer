# fastq_trimmer

## Description
`fastq_trimmer` is a lightweight and high-performance C++ tool for consistently hard-trimming `*.fastq`/`*.fq` (and `*.fastq.gz`/`*.fq.gz`) reads from the 3' (_left_) or 5' (_right_) ends.

## Compilation
Compile the `fastq_trimmer` tool using the following command (as done with Ubuntu Linux):

```sh
g++ -std=c++11 fastq_trimmer.cpp -o fastq_trimmer -lz -pthread -O2
```
## Usage
```sh
./fastq_trimmer --in/-i INPUT_DIRECTORY --out/-o OUTPUT_DIRECTORY [--N3prime/-3] 3_PRIME_TRIM_BASES [--N5prime/-5] 5_PRIME_TRIM_BASES
```
### Options
- `--in` or `-i`: Input directory containing the FastQ files to be trimmed.
- `--out` or `-o`: Output directory where trimmed FastQ files will be saved.
- `--N3prime` or `-3`: 3' end trim value.
- `--N5prime` or `-5`: 5' end trim value.
- `--force` or `-f`: Force processing even if the output file already exists.

## Code Overview
The `fastq_trimmer` tool provides the following functionalities:

- Determines if a file is likely gzipped and accordingly handles I/O.
- Retrieves the list of files with supported extensions (`*.fastq`/`*.fq`/`*.fastq.gz`/`*.fq.gz`) from the input directory.
- Creates the output directory if it doesn't exist.
- Writes messages to a log file within the output directory.
- Trims (both sequence and phred qualities) FastQ files from both gzipped and non-gzipped formats.
- File processing is asynchronously paralellised.

## Compilation
Before compiling, ensure you have the necessary dependencies installed:
- g++ compiler (if not installed: `sudo apt install build-essential`)
- zlib library (`-lz` (if not installed: `sudo apt install zlib1g-dev`))
- POSIX threads (`-pthread` (if not installed: `sudo apt install libpthread-stubs0-dev`))

Compile the tool using the provided command after navigating to a directory containing the source file:
```sh
g++ -std=c++11 fastq_trimmer.cpp -o fastq_trimmer -lz -pthread -O2
```
Alternatively, you may follow this recipe as a guide:
```sh
# Download the archive
git clone https://github.com/QUB-Simpson-lab/fastq_trimmer.git
# Enter the downloaded archive
cd fastq_trimmer
# Compile
g++ -std=c++11 fastq_trimmer.cpp -o fastq_trimmer -lz -pthread -O2
# Install
sudo cp fastq_trimmer /usr/local/bin/
# Usage
fastq_trimmer --in/-i INPUT_DIRECTORY --out/-o OUTPUT_DIRECTORY [--N3prime/-3] 3_PRIME_TRIM_BASES [--N5prime/-5] 5_PRIME_TRIM_BASES
```

## Usage Example
To trim 5 bases from the 3' end and 3 bases from the 5' end of each FastQ file within the `input_dir`, and save the trimmed files in the `output_dir`:
```sh
./fastq_trimmer --in input_dir --out output_dir --N3prime 5 --N5prime 3
```
## License
This tool is provided under the GPLv3 License. See the [LICENSE](LICENSE) file for details.

## Contact
For any questions or issues, please [open an issue here](https://github.com/QUB-Simpson-lab/fastq_trimmer/issues/new/choose) on GitHub.

---
© 2023 [eptroendle](https://github.com/eptroendle) [@QUB-Simpson-lab](https://github.com/QUB-Simpson-lab)
