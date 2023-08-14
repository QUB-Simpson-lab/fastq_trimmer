#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <getopt.h>
#include <zlib.h>
#include <future> // For std::async

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

void writeToLog(const std::string& logFileName, const std::string& message) {
    std::ofstream logFile(logFileName, std::ios_base::app); // Open file in append mode
    if (logFile) {
        logFile << message << std::endl;
    }
}

void trimGzippedFastq(gzFile in, gzFile out, int N) {
    char buffer[4096];
    int lineCount = 0;

    while (gzgets(in, buffer, sizeof(buffer)) != nullptr) {
        std::string line = buffer;

        if (lineCount % 4 == 1 || lineCount % 4 == 3) {
            std::string trimmedLine = line.substr(N);
            gzwrite(out, trimmedLine.c_str(), trimmedLine.size());
        } else {
            gzwrite(out, line.c_str(), line.size());
        }

        lineCount++;
    }
}

void trimNonGzippedFastq(FILE* in, FILE* out, int N) {
    char buffer[4096];
    int lineCount = 0;

    while (fgets(buffer, sizeof(buffer), in) != nullptr) {
        std::string line = buffer;

        if (lineCount % 4 == 1 || lineCount % 4 == 3) {
            std::string trimmedLine = line.substr(N);
            fprintf(out, "%s", trimmedLine.c_str());
        } else {
            fprintf(out, "%s", line.c_str());
        }

        lineCount++;
    }
}

void processFile(const std::string& inputFile, const std::string& outputFile, int N, const std::string& logFileName) {
    writeToLog(logFileName, "Attempting to process " + inputFile + "...");
    if (isLikelyGzipped(inputFile)) {
        gzFile in = gzopen(inputFile.c_str(), "rb");
        gzFile out = gzopen(outputFile.c_str(), "wb");

        if (in && out) {
            trimGzippedFastq(in, out, N);
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
            trimNonGzippedFastq(in, out, N);
            fclose(in);
            fclose(out);

            writeToLog(logFileName, "Processed (non-gzipped): " + inputFile);
        } else {
            writeToLog(logFileName, "Error processing (non-gzipped): " + inputFile);
        }
    }
}

void trimFastqFilesInDirectory(const std::string& inputDirectory, const std::string& outputDirectory, int N) {
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


    std::vector<std::future<void>> tasks;
/*
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // Regular file
            std::string inputFile = inputDirectory + "/" + entry->d_name;
            std::string outputFile = outputDirectory + "/" + entry->d_name;

            tasks.emplace_back(std::async(std::launch::async, processFile, inputFile, outputFile, N, logFileName));
        }
    }

    closedir(dir);

    // Wait for all tasks to complete
    for (auto& task : tasks) {
        task.wait();
    }

    //test sequential to check
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // Regular file
            std::string inputFile = inputDirectory + "/" + entry->d_name;
            std::string outputFile = outputDirectory + "/" + entry->d_name;

            processFile(inputFile, outputFile, N, logFileName);
        }
    }

    closedir(dir);
*/

struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // Regular file
            std::string filename = entry->d_name;
            std::string inputFile = inputDirectory + "/" + filename;
            std::string outputFile = outputDirectory + "/" + filename;

            // Check if the file has a .fastq or .fastq.gz extension
            if (filename.size() >= 6 && (filename.substr(filename.size() - 6) == ".fastq" || filename.substr(filename.size() - 9) == ".fastq.gz")) {
                tasks.emplace_back(std::async(std::launch::async, processFile, inputFile, outputFile, N, logFileName));
            } else {
                writeToLog(logFileName, "Skipped file: " + inputFile + " (unsupported extension)");
            }
        }
    }

    closedir(dir);

    // Wait for all tasks to complete
    for (auto& task : tasks) {
        task.wait();
    }

}

int main(int argc, char* argv[]) {
    std::string inputDirectory;
    std::string outputDirectory;
    int N = -1;

    struct option longOptions[] = {
        {"in", required_argument, nullptr, 'i'},
        {"out", required_argument, nullptr, 'o'},
        {"N", required_argument, nullptr, 'n'},
        {nullptr, 0, nullptr, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "i:o:n:", longOptions, nullptr)) != -1) {
        switch (option) {
            case 'i':
                inputDirectory = optarg;
                break;
            case 'o':
                outputDirectory = optarg;
                break;
            case 'n':
                N = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " --in INPUT_DIRECTORY --out OUTPUT_DIRECTORY --N TRIM_VALUE" << std::endl;
                return 1;
        }
    }

    if (inputDirectory.empty() || outputDirectory.empty() || N < 0) {
        std::cerr << "Missing or invalid arguments. Usage: " << argv[0] << " --in INPUT_DIRECTORY --out OUTPUT_DIRECTORY --N TRIM_VALUE" << std::endl;
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

    trimFastqFilesInDirectory(inputDirectory, outputDirectory, N);

    return 0;
}

