#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <random>
#include <thread>
#include <mutex>

std::mutex entity_mtx;

static const uint32_t NUM_ROWS = 15;

// Constantes
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;

// Probabilidades
const double PLANT_REPRODUCTION_PROBABILITY = 0.2;
const double HERBIVORE_REPRODUCTION_PROBABILITY = 0.075;
const double CARNIVORE_REPRODUCTION_PROBABILITY = 0.025;
const double HERBIVORE_MOVE_PROBABILITY = 0.7;
const double HERBIVORE_EAT_PROBABILITY = 0.9;
const double CARNIVORE_MOVE_PROBABILITY = 0.5;
const double CARNIVORE_EAT_PROBABILITY = 1.0;

// Definição de tipos
enum entity_type_t {
    empty,
    plant,
    herbivore,
    carnivore
};

struct pos_t {
    uint32_t i;
    uint32_t j;
};

struct entity_t {
    entity_type_t type;
    int32_t energy;
    int32_t age;
};

// Código auxilar para converter o entity_type_t enum em uma string
NLOHMANN_JSON_SERIALIZE_ENUM(entity_type_t, {
                                                {empty, " "},
                                                {plant, "P"},
                                                {herbivore, "H"},
                                                {carnivore, "C"},
                                            })

// Código auxiliar para converter o entity_t struct em um objeto JSON
namespace nlohmann {
    void to_json(nlohmann::json &j, const entity_t &e) {
        j = nlohmann::json{{"type", e.type}, {"energy", e.energy}, {"age", e.age}};
    }
}

// Grid (matriz) que contém as enidades
static std::vector<std::vector<entity_t>> entity_grid;

//Constante de entidade vazia
entity_t EMPTY_ENTITIE = {empty, 0, 0};

// Função que gera os conjuntos de entidades
static std::random_device rd;
static std::mt19937 generator(rd());

void createEntitieSet(entity_type_t entitieType, uint32_t nEntities){
    std::uniform_int_distribution<int> randomRow(0, NUM_ROWS - 1);
    std::uniform_int_distribution<int> randomCol(0, NUM_ROWS - 1);

    for (uint32_t i = 0; i < nEntities; ++i){

        int row = randomRow(generator);
        int col = randomCol(generator);

        while (entity_grid[row][col].type != empty){
            row = randomRow(generator);
            col = randomCol(generator);
        }
        entity_grid[row][col] = {entitieType, 100, 0};
    }
}


// Vetor de pares de inteiros que armazena as atualizações
std::vector<std::pair<int, int>> updated_pos;
// Vetor de pares de inteiros que funciona como variável auxiliar
std::vector<std::pair<int, int>> available_pos;

// Função especializada em gerar um booleano randomico, com base em uma probabilidade
bool randomBool(float probability) {
    std::uniform_real_distribution<> distribution(0.0, 1.0);
    return distribution(generator) < probability;
}


void processPlant(int row, int col) {
    entity_mtx.lock();
    // Verifica se a planta tem condições de continuar viva
    if (entity_grid[row][col].age == 10) {
        entity_grid[row][col] = EMPTY_ENTITIE;
    } else {
        entity_grid[row][col].age++;
        // Reprodução
        if (randomBool(PLANT_REPRODUCTION_PROBABILITY)) {
            if ((row + 1) < NUM_ROWS && entity_grid[row + 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row + 1, col));
            }
            if (row > 0 && entity_grid[row - 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row - 1, col));
            }
            if ((col + 1) < NUM_ROWS && entity_grid[row][col + 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col + 1));
            }
            if (col > 0 && entity_grid[row][col - 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int rand_index = distribution(generator);
                int line = available_pos[rand_index].first;
                int column = available_pos[rand_index].second;
                entity_grid[line][column].type = plant;
                updated_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
    }
    entity_mtx.unlock();
}


void processHerbivore(int row, int col) {
    entity_mtx.lock();
    // Verifica se a entidade tem condições de continuar viva
    if (entity_grid[row][col].age == 50 || entity_grid[row][col].energy <= 0) {
        entity_grid[row][col] = EMPTY_ENTITIE;
    } else {
        entity_grid[row][col].age++;
        // Alimentação
        if ((row + 1) < NUM_ROWS && entity_grid[row + 1][col].type == plant && randomBool(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[row + 1][col] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 30;
        }
        if (row > 0 && entity_grid[row - 1][col].type == plant && randomBool(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[row - 1][col] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 30;
        }
        if ((col + 1) < NUM_ROWS && entity_grid[row][col + 1].type == plant && randomBool(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[row][col + 1] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 30;
        }
        if (col > 0 && entity_grid[row][col - 1].type == plant && randomBool(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[row][col - 1] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 30;
        }
        // Reprodução
        if (randomBool(HERBIVORE_REPRODUCTION_PROBABILITY) && entity_grid[row][col].energy >= 20) {
            if ((row + 1) < NUM_ROWS && entity_grid[row + 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row + 1, col));
            }
            if (row > 0 && entity_grid[row - 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row - 1, col));
            }
            if ((col + 1) < NUM_ROWS && entity_grid[row][col + 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col + 1));
            }
            if (col > 0 && entity_grid[row][col - 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int rand_index = distribution(generator);
                int line = available_pos[rand_index].first;
                int column = available_pos[rand_index].second;
                entity_grid[line][column].type = herbivore;
                entity_grid[line][column].energy = 100;
                entity_grid[row][col].energy = entity_grid[row][col].energy - 10;
                updated_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
        // Movimentação
        if (randomBool(HERBIVORE_MOVE_PROBABILITY)) {
            if ((row + 1) < NUM_ROWS && entity_grid[row + 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row + 1, col));
            }
            if (row > 0 && entity_grid[row - 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row - 1, col));
            }
            if ((col + 1) < NUM_ROWS && entity_grid[row][col + 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col + 1));
            }
            if (col > 0 && entity_grid[row][col - 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int rand_index = distribution(generator);
                int line = available_pos[rand_index].first;
                int column = available_pos[rand_index].second;
                entity_grid[line][column] = {herbivore, entity_grid[row][col].energy - 5, entity_grid[row][col].age};
                entity_grid[row][col] = EMPTY_ENTITIE;
                updated_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
    }
    entity_mtx.unlock();
}


void processCarnivore(int row, int col) {
    entity_mtx.lock();
    // Verifica se a entidade tem condições de continuar existindo
    if (entity_grid[row][col].age == 80 || entity_grid[row][col].energy <= 0) {
        entity_grid[row][col] = EMPTY_ENTITIE;
    } else {
        entity_grid[row][col].age++;
        // Alimentação
        if (row + 1 < NUM_ROWS && entity_grid[row + 1][col].type == herbivore && randomBool(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[row + 1][col] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 20;
        }
        if (row > 0 && entity_grid[row - 1][col].type == herbivore && randomBool(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[row - 1][col] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 20;
        }
        if (col + 1 < NUM_ROWS && entity_grid[row][col + 1].type == herbivore && randomBool(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[row][col + 1] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 20;
        }
        if (col > 0 && entity_grid[row][col - 1].type == herbivore && randomBool(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[row][col - 1] = EMPTY_ENTITIE;
            entity_grid[row][col].energy += 20;
        }
        // Reprodução
        if (randomBool(CARNIVORE_REPRODUCTION_PROBABILITY) && entity_grid[row][col].energy >= 20) {
            if ((row + 1) < NUM_ROWS && entity_grid[row + 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row + 1, col));
            }
            if (row > 0 && entity_grid[row - 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row - 1, col));
            }
            if ((col + 1) < NUM_ROWS && entity_grid[row][col + 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col + 1));
            }
            if (col > 0 && entity_grid[row][col - 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int rand_index = distribution(generator);
                int line = available_pos[rand_index].first;
                int column = available_pos[rand_index].second;
                entity_grid[line][column] = {carnivore, 100, 0};
                entity_grid[row][col].energy = entity_grid[row][col].energy - 10;
                updated_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
        // Movimentação
        if (randomBool(CARNIVORE_MOVE_PROBABILITY)) {
            if ((row + 1) < NUM_ROWS && entity_grid[row + 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row + 1, col));
            }
            if (row > 0 && entity_grid[row - 1][col].type == empty) {
                available_pos.push_back(std::make_pair(row - 1, col));
            }
            if ((col + 1) < NUM_ROWS && entity_grid[row][col + 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col + 1));
            }
            if (col > 0 && entity_grid[row][col - 1].type == empty) {
                available_pos.push_back(std::make_pair(row, col - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int rand_index = distribution(generator);
                int line = available_pos[rand_index].first;
                int column = available_pos[rand_index].second;
                entity_grid[line][column] = {carnivore, entity_grid[row][col].energy - 5, entity_grid[row][col].age};
                entity_grid[row][col] = EMPTY_ENTITIE;
                updated_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
    }
    entity_mtx.unlock();
}

int main() {
    crow::SimpleApp app;

    // Endpoint que serve a página HTML
    CROW_ROUTE(app, "/")
    ([](crow::request &, crow::response &res) {
        // Retorna o conteúdo HTML
        res.set_static_file_info_unsafe("../public/index.html");
        res.end(); 
    });

    // Endpoint que inicia a simulação, com os parâmetros estabelecidos
    CROW_ROUTE(app, "/start-simulation").methods("POST"_method)([](crow::request &req, crow::response &res) {
        // Faz o parse no body do JSON
        nlohmann::json request_body = nlohmann::json::parse(req.body);
        // Valida o número total de entidades no body
        uint32_t total_entities = (uint32_t)request_body["plants"] + (uint32_t)request_body["herbivores"] + (uint32_t)request_body["carnivores"];
        if (total_entities > NUM_ROWS * NUM_ROWS) {
            res.code = 400;
            res.body = "Too many entities";
            res.end();
            return;
        }
        // Limpa o grid de entidades
        entity_grid.clear();
        entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_ROWS, { empty, 0, 0 }));
        std::uniform_int_distribution<> distribution(0, 14);

        // Dentro do endpoint "/start-simulation"
        createEntitieSet(plant, (uint32_t)request_body["plants"]);
        createEntitieSet(herbivore, (uint32_t)request_body["herbivores"]);
        createEntitieSet(carnivore, (uint32_t)request_body["carnivores"]);

        // Retorna o JSON que representa o grid de entidades
        nlohmann::json json_grid = entity_grid;
        res.body = json_grid.dump();
        res.end();
    });

    // Endpoint para avançar a simulação para a próxima iteração
    CROW_ROUTE(app, "/next-iteration").methods("GET"_method)([]() {
        bool updated = false;
        for (uint32_t raw = 0; raw < NUM_ROWS; raw++) {
            for (uint32_t col = 0; col < NUM_ROWS; col++) {
                // Verifica no vetor de atualizações se a posição do grid está
                updated = false;
                for (int i = 0; i < updated_pos.size(); i++) {
                    if (updated_pos[i].first == raw && updated_pos[i].second == col) {
                        updated = true;
                    }
                }
                // Lida com a atualização da posição do grid
                if (!updated) {
                    if (entity_grid[raw][col].type == plant) {
                        std::thread tplant(processPlant, raw, col);
                        tplant.join();
                    } 
                    else if (entity_grid[raw][col].type == herbivore) {
                        std::thread therbivore(processHerbivore, raw, col);
                        therbivore.join();
                    } 
                    else if (entity_grid[raw][col].type == carnivore) {
                        std::thread tcarnivore(processCarnivore, raw, col);
                        tcarnivore.join();
                    }
                }
            }
        }
        updated_pos.clear();
        // Retorna a representação do grid em JSON
        nlohmann::json json_grid = entity_grid; 
        return json_grid.dump(); 
    });
    // Roda o servidor
    app.port(8080).run();
    return 0;
}
