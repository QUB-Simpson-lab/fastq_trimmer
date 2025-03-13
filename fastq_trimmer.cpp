#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <dirent.h>
#include <getopt.h>
#include <unistd.h>
#include <zlib.h>

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

void trimGzippedFastq(gzFile in, gzFile out, gzFile out3p, gzFile out5p,
                      const int& N3, const int& N5, const bool& keep) {
    char buffer[4096];
    int lineCount = 0;
    while (gzgets(in, buffer, sizeof(buffer)) != nullptr) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';  // Remove newline to work with trimming
            --len;
        }

        if (lineCount % 4 == 1 || lineCount % 4 == 3) { // Sequence or quality line
            char* trimmedStart = buffer + N5;  // Move pointer for 5' trim
            size_t trimmedLen = (len > N3 + N5) ? (len - N3 - N5) : 0;

            gzwrite(out, trimmedStart, trimmedLen);
            gzwrite(out, "\n", 1);  // Ensure newline

            if (keep) {
                if (out5p && N5 > 0) {
                    gzwrite(out5p, buffer, N5);
                    gzwrite(out5p, "\n", 1);
                }
                if (out3p && N3 > 0) {
                    gzwrite(out3p, buffer + len - N3, N3);
                    gzwrite(out3p, "\n", 1);
                }
            }
        } else {
            gzwrite(out, buffer, len);
            gzwrite(out, "\n", 1);
            if (keep) {
                if (out5p && N5 > 0) {
                    gzwrite(out5p, buffer, len);
                    gzwrite(out5p, "\n", 1);
                }
                if (out3p && N3 > 0) {
                    gzwrite(out3p, buffer, len);
                    gzwrite(out3p, "\n", 1);
                }
            }
        }
        lineCount++;
    }
}

void trimNonGzippedFastq(FILE* in, FILE* out, FILE* out3p, FILE* out5p, const int& N3, const int& N5, const bool& keep) {
    char buffer[4096];
    int lineCount = 0;

    while (fgets(buffer, sizeof(buffer), in) != nullptr) {
        size_t len = strlen(buffer);

        // Ensure we keep the newline character
        bool hasNewline = (len > 0 && buffer[len - 1] == '\n');
        if (hasNewline) {
            buffer[len - 1] = '\0';  // Temporarily remove newline for processing
            len--;
        }

        if (lineCount % 4 == 1 || lineCount % 4 == 3) {
            char* trimmedStart = buffer + N5;
            size_t trimmedLen = (len > N3 + N5) ? (len - N3 - N5) : 0;

            if (keep) {
                // Write trimmed off 5' sequence
                if (out5p && N5 > 0) {
                    fprintf(out5p, "%.*s\n", (int)std::min(len, (size_t)N5), buffer);
                }
                // Write trimmed off 3' sequence
                if (out3p && N3 > 0) {
                    size_t trim3Start = len > N3 ? len - N3 : 0;
                    fprintf(out3p, "%s\n", buffer + trim3Start);
                }
            }

            fprintf(out, "%.*s\n", (int)trimmedLen, trimmedStart);
        } else {
            fprintf(out, "%s\n", buffer);
            if (keep) {
                if (out5p && N5 > 0) {
                    fprintf(out5p, "%s\n", buffer);
                }
                if (out3p && N3 > 0) {
                    fprintf(out3p, "%s\n", buffer);
                }
            }
        }

       lineCount++;
    }
}

void processFile(const std::string& inputDirectory, const std::string& outputDirectory, const std::string& filename, const int& N3, const int& N5, const std::string& logFileName, const bool& keep) {
    std::string inputFile = inputDirectory + "/" + filename;
    std::string outputFile = outputDirectory + "/" + filename;
    writeToLog(logFileName, "Attempting to process " + inputFile + " and write to " + outputFile + " ...");

    if (isLikelyGzipped(inputFile)) {
        gzFile in = gzopen(inputFile.c_str(), "rb");
        gzFile out = gzopen(outputFile.c_str(), "wb");
        gzFile out3p = nullptr;
        gzFile out5p = nullptr;
        if (keep) {
            if (N3 > 0) {
                std::string trim3File = (outputDirectory + "/3-prime/trim3_" + filename);
                out3p = gzopen(trim3File.c_str(), "wb");
            }
            if (N5 > 0) {
                std::string trim5File = (outputDirectory + "/5-prime/trim5_" + filename);
                out5p = gzopen(trim5File.c_str(), "wb");
            }
        }
        if (in && out) {
            trimGzippedFastq(in, out, out3p, out5p, N3, N5, keep);
            gzclose(in);
            gzclose(out);
            if (keep) {
                if(out3p) {
                    gzclose(out3p);
                }
                if(out5p) {
                    gzclose(out5p);
                }
            }
            writeToLog(logFileName, "Processed (gzipped): " + inputFile);
        }
        else {
            writeToLog(logFileName, "Error processing (gzipped): " + inputFile);
        }
    } else {
        FILE* in = fopen(inputFile.c_str(), "r");
        FILE* out = fopen(outputFile.c_str(), "w");
        FILE* out3p = nullptr;
        FILE* out5p = nullptr;
        if (keep) {
            std::string trim5File = (outputDirectory + "/5-prime/trim5_" + filename);
            std::string trim3File = (outputDirectory + "/3-prime/trim3_" + filename);
            out3p = fopen(trim3File.c_str(), "w");
            out5p = fopen(trim5File.c_str(), "w");
        }
        if (in && out) {
            trimNonGzippedFastq(in, out, out3p, out5p, N3, N5, keep);
            fclose(in);
            fclose(out);

            if (keep) {
                if (out3p) {
                    fclose(out3p);
                }
                if (out5p) {
                fclose(out5p);
                }
            }
            writeToLog(logFileName, "Processed (non-gzipped): " + inputFile);
        } else {
            writeToLog(logFileName, "Error processing (non-gzipped): " + inputFile);
        }
    }
}

void trimFastqFilesInDirectory(const std::string& inputDirectory, const std::string& outputDirectory, int N3, int N5, bool force, bool keep) {

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

        if (keep) {
            std::string outputDirectory3Prime = outputDirectory + "/3-prime";
            std::string outputDirectory5Prime = outputDirectory + "/5-prime";
            if (N3 > 0) {
                if (access(outputDirectory3Prime.c_str(), F_OK) == -1) {
                    if (mkdir(outputDirectory3Prime.c_str(), 0777) != 0) {
                        std::cerr << "Error creating output directory: " << outputDirectory3Prime << std::endl;
                        closedir(dir);
                        return;
                    }
                }
            }
            if (N5 > 0) {
                if (access(outputDirectory5Prime.c_str(), F_OK) == -1) {
                    if (mkdir(outputDirectory5Prime.c_str(), 0777) != 0) {
                        std::cerr << "Error creating output directory: " << outputDirectory5Prime << std::endl;
                        closedir(dir);
                        return;
                    }
                }
            }
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
    std::size_t numCores = std::thread::hardware_concurrency();

    // Get a list of filenames with supported extensions from the input directory
    std::vector<std::string> supportedExtensions = { ".fq", ".fq.gz", ".fastq", ".fastq.gz" };
    std::vector<std::string> fileNames = getFilesWithExtensions(inputDirectory, supportedExtensions);

    std::size_t totalIterations = fileNames.size();
    std::size_t completedIterations = 0;


    for (const std::string& filename : fileNames) {
        std::string outputFile = outputDirectory + "/" + filename;
        bool file_exists = (access(outputFile.c_str(), F_OK) == 0);
        //bool file_exists = std::ifstream(output_file_name).good();

        if (!force && file_exists) {
            std::cout << "Output file already exists for " << filename << ", skipping..." << std::endl;
            ++completedIterations;
            writeToLog(logFileName, "Skipped file: " + filename + " (output file already exists)");
            continue; // Skip processing this iteration
        }

        if (futures.size() >= numCores) {
            futures.front().wait();
            futures.erase(futures.begin());
        }

        std::cout << "Processing: " << filename << " (N5 = " << N5 << ", N3 = " << N3 << ")" << std::endl;

        futures.push_back(std::async(std::launch::async, [&completedIterations, totalIterations, inputDirectory, outputDirectory, filename, N3, N5, logFileName, keep]() {
                processFile(inputDirectory, outputDirectory, filename, N3, N5, logFileName, keep);
                ++completedIterations;
                std::cout << "Progress: " << completedIterations << "/" << totalIterations << " iterations" << std::endl;
        }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }
    closedir(dir);
    return;
}

int main(int argc, char* argv[]) {
    std::cout << "Welcome to fastq_trimmer v0.2" << std::endl;
    std::cout << "Program: " << argv[0] << std::endl;
    std::cout << "Command-line arguments:" << std::endl;
    for (int i = 1; i < argc; ++i) {
        std::cout << "  " << argv[i] << std::endl;
    }
    std::string inputDirectory = "";
    std::string outputDirectory = "";
    int N3 = 0;
    int N5 = 0;
    bool force = false;
    bool keep = false;
    struct option longOptions[] = {
        {"in", required_argument, nullptr, 'i'},
        {"out", required_argument, nullptr, 'o'},
        {"N3prime", required_argument, nullptr, '3'},
        {"N5prime", required_argument, nullptr, '5'},
        {"force", no_argument, nullptr, 'f'},
        {"keep", no_argument, nullptr, 'k'},
        {nullptr, 0, nullptr, 0}
    };
    int option;
    int optionIndex = 0;

    while ((option = getopt_long(argc, argv, "i:o:3:5:fk", longOptions, &optionIndex)) != -1) {
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
            case 'k':
                std::cout << 'k' << std::endl;
                keep = true;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " --in/-i INPUT_DIRECTORY --out/-o OUTPUT_DIRECTORY [--N3prime/-3] 3_PRIME_TRIM_VALUE [--N5prime/-5] 5_PRIME_TRIM_VALUE [--force/-f] [--keep/-k]" << std::endl;
                return 1;
        }
    }

    std::cout << "Input Directory: " << inputDirectory << std::endl;
    std::cout << "Output Directory: " << outputDirectory << std::endl;
    std::cout << "N3 Prime Trim Value: " << N3 << std::endl;
    std::cout << "N5 Prime Trim Value: " << N5 << std::endl;
    std::cout << "Force Flag: " << (force ? "true" : "false") << std::endl;
    std::cout << "Keep Flag: " << (keep ? "true" : "false") << std::endl;

    if (inputDirectory.empty() || outputDirectory.empty() || (N3 < 0) || ( N5 < 0) || (N3 == 0 && N5 == 0) ) {
        std::cerr << "Missing or invalid arguments. Usage: " << argv[0] << " --in/-i INPUT_DIRECTORY --out/-o OUTPUT_DIRECTORY [--N3prime/-3] 3_PRIME_TRIM_VALUE [--N5prime/-5] 5_PRIME_TRIM_VALUE" << std::endl;
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

    if (keep) {
        if (N3 > 0) {
            if (!createDirectoryIfNotExists(outputDirectory + "/3-prime")) {
                return 1;
            }
        }
        if (N5 > 0) {
            if (!createDirectoryIfNotExists(outputDirectory + "/5-prime")) {
                return 1;
            }
        }
    }

    // Check if output directory is the same as the input directory
    char* resolvedInputPath = realpath(inputDirectory.c_str(), nullptr);
    char* resolvedOutputPath = realpath(outputDirectory.c_str(), nullptr);

    if (resolvedInputPath == nullptr || resolvedOutputPath == nullptr) {
        std::cerr << "Error resolving paths. Exiting..." << std::endl;
        free(resolvedInputPath);
        free(resolvedOutputPath);
        return 1;
    }

    bool arePathsEqual = (strcmp(resolvedInputPath, resolvedOutputPath) == 0);

    free(resolvedInputPath);
    free(resolvedOutputPath);

    if (arePathsEqual) {
        std::cerr << "Input and output directories cannot be the same." << std::endl;
        return 1;
    }

    trimFastqFilesInDirectory(inputDirectory, outputDirectory, N3, N5, force, keep);

    return 0;
}