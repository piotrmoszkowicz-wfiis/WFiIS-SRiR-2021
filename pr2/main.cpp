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

        std::map<std::string, std::vector<int>> results;

        int task = 0;

        upcxx::future<> f = upcxx::make_future();

        for (const auto &fileName : files) {
            f = upcxx::when_all(f, upcxx::rpc(1 + (task % upcxx::rank_n()),
                                              [](std::string fileName, int dictionarySize,
                                                 std::vector<std::string> dictionaryItems) {
                                                  std::map<std::string, int> singleFileResult;
                                                  std::vector<int> valuesVector;

                                                  std::string word;
                                                  std::ifstream readFile;

                                                  readFile.open(fileName);

                                                  while (readFile >> word) {
                                                      ++singleFileResult[word];
                                                  }

                                                  for (unsigned int i = 0; i < dictionarySize; i++) {
                                                      int value = 0;
                                                      try {
                                                          value = singleFileResult.at(dictionaryItems[i]);
                                                      } catch (std::exception &e) {}
                                                      valuesVector.push_back(value);
                                                  }

                                                  return std::make_pair(fileName, valuesVector);
                                              }, fileName, dictionarySize, dictionaryItems).then(
                    [&results](std::pair<std::string, std::vector<int>> result) {
                        results[result.first] = result.second;
                    }));
            task++;
        }

        f.wait();

        std::cout << "Zaczynam zapisywanie wyniku..." << std::endl;

        // Zapisz wynik
        std::vector<int> finalResult;

        finalResult.resize(dictionarySize);

        for (const auto& mapItem : results) {
            std::cout << "Plik " << mapItem.first << std::endl;
            int i = 0;
            for (const auto& singleWordInFile : mapItem.second) {
                std::cout << " Slowo " << dictionaryItems.at(i) << " wystapien " << singleWordInFile << std::endl;
                finalResult[i++] += singleWordInFile;
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
