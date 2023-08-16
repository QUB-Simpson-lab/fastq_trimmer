#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <getopt.h>
#include <zlib.h>
#include <future>

bool isLikelyGzipped(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }

    unsigned char magic[2];
    file.read(reinterpret_cast<char*>(magic), sizeof(magic));

    return (file && magic[0] == 0x1F && magic[1] == 0x8B);
}

bool createDirectoryIfNotExists(const std::string& directoryPath) {
    struct stat st;
    if (stat(directoryPath.c_str(), &st) == -1) {
        if (mkdir(directoryPath.c_str(), 0777) == 0) {
            return true; // Directory created successfully
        } else {
            std::cerr << "Error creating directory: " << directoryPath << std::endl;
            return false;
        }
    }
    return true; // Directory already exists
}

std::vector<std::string> getFilesWithExtensions(const std::string& directory, const std::vector<std::string>& extensions) {
    std::vector<std::string> fileNames;
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        return fileNames;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // Regular file
            std::string filename = entry->d_name;
            for (const std::string& extension : extensions) {
                if (filename.size() >= extension.size() && filename.substr(filename.size() - extension.size()) == extension) {
                    fileNames.push_back(filename);
                    break; // Break once an extension is found
                }
            }
        }
    }

    closedir(dir);
    return fileNames;
}

void writeToLog(const std::string& logFileName, const std::string& message) {
    std::ofstream logFile(logFileName, std::ios_base::app); // Open file in append mode
    if (logFile) {
        logFile << message << std::endl;
    }
}

void trimGzippedFastq(gzFile in, gzFile out, int N3, int N5) {
    char buffer[4096];
    int lineCount = 0;

    while (gzgets(in, buffer, sizeof(buffer)) != nullptr) {
        std::string line = buffer;

        if (lineCount % 4 == 1 || lineCount % 4 == 3) {
            // Trim from the 3' end (N3)
            std::string trimmedLine = line.substr(N3);
             if (N5 > 0) {
                size_t length = trimmedLine.length();
                if (length > N5) {
                    trimmedLine = trimmedLine.substr(0, length - N5);
                } else {
                    trimmedLine.clear();
                }
            }
            gzwrite(out, trimmedLine.c_str(), trimmedLine.size());
        } else {
            gzwrite(out, line.c_str(), line.size());
        }

        lineCount++;
    }
}

void trimNonGzippedFastq(FILE* in, FILE* out, int N3, int N5) {
    char buffer[4096];
    int lineCount = 0;

    while (fgets(buffer, sizeof(buffer), in) != nullptr) {
        std::string line = buffer;

        if (lineCount % 4 == 1 || lineCount % 4 == 3) {
            // Trim from the 3' end (N3)
            std::string trimmedLine = line.substr(N3);
            // Trim from the 5' end (N5)
            if (N5 > 0) {
                size_t length = trimmedLine.length();
                if (length > N5) {
                    trimmedLine = trimmedLine.substr(0, length - N5);
                } else {
                    trimmedLine.clear();
                }
            }
            fprintf(out, "%s", trimmedLine.c_str());
        } else {
            fprintf(out, "%s", line.c_str());
        }

        lineCount++;
    }
}

void processFile(const std::string& inputFile, const std::string& outputFile, int N3, int N5, const std::string& logFileName) {
    writeToLog(logFileName, "Attempting to process " + inputFile + "...");
    if (isLikelyGzipped(inputFile)) {
        gzFile in = gzopen(inputFile.c_str(), "rb");
        gzFile out = gzopen(outputFile.c_str(), "wb");

        if (in && out) {
            trimGzippedFastq(in, out, N3, N5);
            gzclose(in);
            gzclose(out);

            writeToLog(logFileName, "Processed (gzipped): " + inputFile);
        }
        else {
            writeToLog(logFileName, "Error processing (gzipped): " + inputFile);
        }
    } else {
        FILE* in = fopen(inputFile.c_str(), "r");
        FILE* out = fopen(outputFile.c_str(), "w");

        if (in && out) {
            trimNonGzippedFastq(in, out, N3, N5);
            fclose(in);
            fclose(out);

            writeToLog(logFileName, "Processed (non-gzipped): " + inputFile);
        } else {
            writeToLog(logFileName, "Error processing (non-gzipped): " + inputFile);
        }
    }
}

void trimFastqFilesInDirectory(const std::string& inputDirectory, const std::string& outputDirectory, int N3, int N5, bool force) {

    DIR* dir = opendir(inputDirectory.c_str());
    if (!dir) {
        std::cerr << "Error opening input directory: " << inputDirectory << std::endl;
        return;
    }

    if (access(outputDirectory.c_str(), F_OK) == -1) {
        if (mkdir(outputDirectory.c_str(), 0777) != 0) {
            std::cerr << "Error creating output directory: " << outputDirectory << std::endl;
            closedir(dir);
            return;
        }
    }

    // Create log file
    std::string logFileName = outputDirectory + "/log.txt";
    std::ofstream logFile(logFileName);
    if (!logFile) {
        std::cerr << "Error creating log file: " << logFileName << std::endl;
        return;
    }

    // Parallel processing using std::async and std::thread
    std::vector<std::future<void>> futures;
    std::size_t numCores = std::thread::hardware_concurrency() - 1;

    // Get a list of filenames with supported extensions from the input directory
    std::vector<std::string> supportedExtensions = { ".fq", ".fq.gz", ".fastq", ".fastq.gz" };
    std::vector<std::string> fileNames = getFilesWithExtensions(inputDirectory, supportedExtensions);

    std::size_t totalIterations = fileNames.size();
    std::size_t completedIterations = 0;


    for (const std::string& filename : fileNames) {
        std::string inputFile = inputDirectory + "/" + filename;
        std::string outputFile = outputDirectory + "/" + filename;
        bool file_exists = (access(outputFile.c_str(), F_OK) == 0);
        //bool file_exists = std::ifstream(output_file_name).good();

        if (!force && file_exists) {
            std::cout << "Output file already exists for " << filename << ", skipping..." << std::endl;
            ++completedIterations;
            writeToLog(logFileName, "Skipped file: " + inputFile + " (output file already exists)");
            continue; // Skip processing this iteration
        }

        if (futures.size() >= numCores) {
            futures.front().wait();
            futures.erase(futures.begin());
        }

        std::cout << "Processing: " << filename << " (N3 = " << N3 << ", N5 = " << N5 << ")" << std::endl;

        futures.push_back(std::async(std::launch::async, [&completedIterations, totalIterations, inputFile, outputFile, N3, N5, logFileName]() {
                processFile(inputFile, outputFile, N3, N5, logFileName);
                ++completedIterations;
                std::cout << "Progress: " << completedIterations << "/" << totalIterations << " iterations" << std::endl;
        }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }
}

int main(int argc, char* argv[]) {
    std::string inputDirectory;
    std::string outputDirectory;
    int N3 = 0;
    int N5 = 0;
    bool force = false;
    struct option longOptions[] = {
        {"in", required_argument, nullptr, 'i'},
        {"out", required_argument, nullptr, 'o'},
        {"N3prime", optional_argument, nullptr, '3'},
        {"N5prime", optional_argument, nullptr, '5'},
        {"force", optional_argument, nullptr, 'f'},
        {nullptr, 0, nullptr, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "i:o:3:5:f", longOptions, nullptr)) != -1) {
        switch (option) {
            case 'i':
                inputDirectory = optarg;
                break;
            case 'o':
                outputDirectory = optarg;
                break;
            case '3':
                N3 = std::stoi(optarg);
                break;
            case '5':
                N5 = std::stoi(optarg);
                break;
            case 'f':
                force = true;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " --in INPUT_DIRECTORY --out OUTPUT_DIRECTORY [--N3prime/-3] 3_PRIME_TRIM_VALUE [--N5prime/-5] 5_PRIME_TRIM_VALUE" << std::endl;
                return 1;
        }
    }

    if (inputDirectory.empty() || outputDirectory.empty() || (N3 < 0) || ( N5 < 0) || (N3 == 0 && N5 == 0) ) {
        std::cerr << "Missing or invalid arguments. Usage: " << argv[0] << " --in INPUT_DIRECTORY --out OUTPUT_DIRECTORY [--N3prime/-3] 3_PRIME_TRIM_VALUE [--N5prime/-5] 5_PRIME_TRIM_VALUE" << std::endl;
        return 1;
    }

    // Check if input directory exists
    if (access(inputDirectory.c_str(), F_OK) == -1) {
        std::cerr << "Input directory does not exist: " << inputDirectory << std::endl;
        return 1;
    }

   if (!createDirectoryIfNotExists(outputDirectory)) {
        return 1;
   }

    trimFastqFilesInDirectory(inputDirectory, outputDirectory, N3, N5, force);

    return 0;
}
