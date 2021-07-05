//
// Created by Piotr Moszkowicz on 20/06/2021.
//

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <mpi.h>

enum MessageTags {
    DICT_SIZE = 0,
    FILENAME = 1,
    RESULT_VECTOR = 2,
    EMPTY = 3,
};

void doTheMasterStuff(int argc, char* argv[], int nodesCount) {
    // Poczekaj na wiadomość od worker0 z słownikiem, w tle odczytuj pliki
    int dictionarySize;
    MPI_Request dictionarySizeRequest;
    MPI_Status status;

    MPI_Irecv(&dictionarySize, 1, MPI_INT, MPI_ANY_SOURCE, MessageTags::DICT_SIZE, MPI_COMM_WORLD, &dictionarySizeRequest);
    // Manager => Odczytywanie plików

    std::vector<std::string> files;

    std::cout << "Wczytuje liste plikow" << std::endl;

    for (const auto& file : std::filesystem::directory_iterator(".")) {
        const auto ext = std::string(file.path().extension());
        if (ext == ".txt" || ext == ".tex" || ext == ".html") {
            files.push_back(file.path());
        }
    }

    std::cout << "Wczytalem pliki. Czekam na informacje o slowniku" << std::endl;

    // Poczekaj na słownik
    MPI_Wait(&dictionarySizeRequest, &status);

    std::cout << "Mam informacje o slowniku, alokuje potrzebne miejsca w pamieci" << std::endl;

    auto resultBuffer = new int[dictionarySize];

    std::vector<std::vector<int>> results;

    results.resize(files.size());

    for (auto& fileResults : results) {
        fileResults.resize(dictionarySize);
    }

    int terminatedProcesses = 0;
    int filesAssigned = 0;

    int currentMessageSource;
    int currentMessageTag;

    auto assigned = new int[nodesCount];

    do {
        MPI_Recv(resultBuffer, dictionarySize, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        currentMessageSource = status.MPI_SOURCE;
        currentMessageTag = status.MPI_TAG;

        std::cout << "Otrzymalem wiadomosc od " << currentMessageSource << " Tag: " << currentMessageTag << std::endl;

        if (currentMessageTag == MessageTags::RESULT_VECTOR) {
            // Zapisz rezultat
            for (unsigned int i = 0; i < dictionarySize; i++) {
                results[assigned[currentMessageSource]][i] = resultBuffer[i];
            }
        }

        if (filesAssigned < files.size()) {
            // Jest praca do wykonania - przydziel
            MPI_Send(files[filesAssigned].data(), static_cast<int>(files[filesAssigned].size()), MPI_CHAR, currentMessageSource, MessageTags::FILENAME, MPI_COMM_WORLD);

            std::cout << "Przydzielilem plik " << files[filesAssigned] << " dla " << currentMessageSource << std::endl;

            assigned[currentMessageSource] = filesAssigned;
            filesAssigned++;
        } else {
            // Nie ma wiecej pracy - terminuj
            MPI_Send(nullptr, 0, MPI_CHAR, currentMessageSource, MessageTags::FILENAME, MPI_COMM_WORLD);

            std::cout << "Kaze sie wylaczyc node'owi " << currentMessageSource << "wyl/ilosc" << terminatedProcesses << "/" << (nodesCount - 1) << std::endl;

            terminatedProcesses++;
        }
    } while(terminatedProcesses < (nodesCount - 1));

    delete [] resultBuffer;
    delete [] assigned;

    // Write result
    // TODO: Save to file
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
}

void doTheWorkerStuff(int argc, char* argv[], MPI_Comm workerCommunicator) {
    long int dictionarySize;
    int fileNameLength;
    int workerId;

    MPI_Request sendRequest;
    MPI_Status status;

    std::string encodedDictionary;
    std::vector<std::string> dictionaryItems;
    std::map<std::string, int> singleFileResult;

    MPI_Comm_rank(workerCommunicator, &workerId);

    // Jestem gotowy do przyjecia pracy
    MPI_Isend(nullptr, 0, MPI_UNSIGNED_CHAR, 0, MessageTags::EMPTY, MPI_COMM_WORLD, &sendRequest);

    if (workerId == 0) {
        // Worker 0 => przeczytaj slownik
        std::ifstream dictionaryStream;

        dictionaryStream.open("dict.txt", std::ifstream::in);

        for (std::string word; std::getline(dictionaryStream, word); ) {
            dictionaryItems.push_back(word);
            encodedDictionary += word += "|";
        }

        encodedDictionary.erase(encodedDictionary.size() - 1);

        dictionarySize = static_cast<long int>(dictionaryItems.size());
    }

    MPI_Bcast(&dictionarySize, 1, MPI_LONG, 0, workerCommunicator);
    MPI_Bcast(encodedDictionary.data(), static_cast<int>(encodedDictionary.size()), MPI_CHAR, 0, workerCommunicator);

    if (workerId != 0) {
        // Zbuduj dictionaryItems z wiadomosci
        dictionaryItems.resize(dictionarySize);

        std::string dictItem;
        std::stringstream sStream(encodedDictionary);

        while(std::getline(sStream, dictItem, '|')) {
            dictionaryItems.push_back(dictItem);
        }
    }

    if (workerId == 0) {
        // Wyslij dlugosc slownika do managera
        MPI_Send(&dictionarySize, 1, MPI_INT, 0, MessageTags::DICT_SIZE, MPI_COMM_WORLD);
    }

    while (true) {
        MPI_Probe(0, MessageTags::FILENAME, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_CHAR, &fileNameLength);

        if (fileNameLength == 0) {
            // Nie ma juz wiecej pracy do wykonania - terminujemy
            break;
        }

        std::string fileNameToRead;
        fileNameToRead.resize(fileNameLength);

        MPI_Recv(fileNameToRead.data(), static_cast<int>(fileNameToRead.size()), MPI_CHAR, 0, MessageTags::FILENAME, MPI_COMM_WORLD, &status);

        std::string word;
        std::ifstream readFile;

        readFile.open(fileNameToRead);

        while (readFile >> word) {
            ++singleFileResult[word];
        }

        std::vector<int> valuesVector;
        valuesVector.resize(dictionarySize);

        for (auto & it : singleFileResult) {
            valuesVector.push_back(it.second);
        }

        MPI_Send(valuesVector.data(), static_cast<int>(dictionarySize), MPI_INT, 0, MessageTags::RESULT_VECTOR, MPI_COMM_WORLD);

        singleFileResult.clear();
    }
}

int main(int argc, char* argv[]) {
    int nodesCount;
    int nodeId;
    MPI_Comm workerCommunicator;

    constexpr unsigned int rootId = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nodesCount);
    MPI_Comm_rank(MPI_COMM_WORLD, &nodeId);

    if (nodeId == rootId) {
        MPI_Comm_split(MPI_COMM_WORLD, MPI_UNDEFINED, nodeId, &workerCommunicator);
        doTheMasterStuff(argc, argv, nodesCount);
    } else {
        MPI_Comm_split(MPI_COMM_WORLD, 0, nodeId, &workerCommunicator);
        doTheWorkerStuff(argc, argv, workerCommunicator);
    }

    MPI_Finalize();
    return 0;
}
