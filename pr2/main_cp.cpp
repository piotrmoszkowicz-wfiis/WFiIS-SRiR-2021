//
// Created by Piotr Moszkowicz on 20/06/2021.
//

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <upcxx/upcxx.hpp>

int main(int argc, char *argv[]) {
    upcxx::init();

    std::vector<std::string> dictionaryItems;

    constexpr unsigned int rootId = 0;
    int dictionarySize;

    if (upcxx::rank_me() == rootId) {
        std::cout << "Czytam slownik..." << std::endl;
        std::ifstream dictionaryStream;

        dictionaryStream.open("dict.txt", std::ifstream::in);

        for (std::string word; std::getline(dictionaryStream, word);) {
            std::cout << "Wyr: " << word << std::endl;
            dictionaryItems.push_back(word);
        }

        dictionarySize = static_cast<int>(dictionaryItems.size());
    }

    if (upcxx::rank_me() == rootId) {
        std::cout << "Czytam pliki" << std::endl;
        std::vector<std::string> files;

        for (const auto &file : std::filesystem::directory_iterator(".")) {
            const auto name = std::string(file.path().filename());
            const auto ext = std::string(file.path().extension());
            if (name != "dict.txt" && (ext == ".txt" || ext == ".tex" || ext == ".html")) {
                std::cout << "Plik " << file.path() << std::endl;
                files.push_back(file.path());
            }
        }

        std::vector<std::vector<int>> results;

        results.resize(files.size());

        for (auto &fileResults : results) {
            fileResults.resize(dictionarySize);
        }

        int task = 0;

        for (const auto &fileName : files) {
            std::vector<int> localResult = upcxx::rpc(1 + (task % upcxx::rank_n()),
                                                      [](std::string fileName, int dictionarySize,
                                                         std::vector<std::string> dictionaryItems) {
                                                          std::cout << "Licze w lambdzie, proces " << upcxx::rank_me()
                                                                    << std::endl;
                                                          std::map<std::string, int> singleFileResult;
                                                          std::vector<int> valuesVector;

                                                          std::string word;
                                                          std::ifstream readFile;

                                                          std::cout << "Plik o nazwie " << fileName << std::endl;

                                                          std::string fileNameString(fileName);

                                                          std::cout << "Otwieram plik..." << fileName << " "
                                                                    << fileNameString << std::endl;

                                                          readFile.open(fileNameString);

                                                          std::cout << "Licze wyrazy..." << std::endl;

                                                          while (readFile >> word) {
                                                              ++singleFileResult[word];
                                                          }

                                                          std::cout << "Tworze wektor odpowiedz..." << std::endl;

                                                          for (unsigned int i = 0; i < dictionarySize; i++) {
                                                              int value = 0;
                                                              try {
                                                                  value = singleFileResult.at(dictionaryItems[i]);
                                                              } catch (std::exception &e) {}
                                                              valuesVector.push_back(value);
                                                          }

                                                          std::cout << "Zwracam rezultat..." << std::endl;

                                                          return valuesVector;
                                                      }, fileName, dictionarySize, dictionaryItems).wait();
            results[task] = localResult;
            task++;
        }

        std::cout << "Zaczynam zapisywanie wyniku..." << std::endl;

        // Zapisz wynik
        std::vector<int> finalResult;

        finalResult.resize(dictionarySize);

        for (unsigned int i = 0; i < results.size(); i++) {
            std::cout << files.at(i) << std::endl;
            for (unsigned int j = 0; j < dictionarySize; j++) {
                std::cout << j << " " << results.at(i).at(j) << std::endl;
                finalResult[j] += results.at(i).at(j);
            }
        }

        for (unsigned int i = 0; i < finalResult.size(); i++) {
            std::cout << "Slowo " << i << " wystapien " << finalResult.at(i) << std::endl;
        }

        std::ofstream resultsFile;
        resultsFile.open("result.txt");
        for (auto res : finalResult) {
            resultsFile << res << std::endl;
        }
        resultsFile.close();
    }

    upcxx::finalize();
    return 0;
}
