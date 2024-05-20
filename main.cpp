#include <iostream>
#include <utility>
#include <vector>
#include <algorithm>
#include <cctype>
#include <memory>
#include <fstream>
#include <sstream>
#include <functional>
#include <limits>

struct Game
{
    std::string Id;
    std::string name;
    std::string gender;
    std::vector<std::string> stores;

    Game() = default;

    Game(const std::string &name, std::string gender, const std::vector<std::string> &stores)
        : name(name), gender(std::move(gender)), stores(stores)
    {
        Id = name;
        Id.erase(std::remove_if(Id.begin(), Id.end(), [](unsigned char c)
                                { return !std::isalnum(c); }),
                 Id.end());
        std::transform(Id.begin(), Id.end(), Id.begin(), [](unsigned char c)
                       { return std::tolower(c); });
    }

    void display() const
    {
        std::cout << "Id: " << Id << "\n"
                  << "Nome: " << name << "\n"
                  << "Genero: " << gender << "\n"
                  << "Lojas: ";
        for (const auto &store : stores)
        {
            std::cout << store << ", ";
        }
        std::cout << std::endl;
    }
};

class BTreeNode
{
    std::vector<Game> games;
    std::vector<std::unique_ptr<BTreeNode>> children;
    bool leaf;
    int t;

public:
    BTreeNode(bool leaf, int t) : leaf(leaf), t(t) {}

    void traverse() const;

    [[nodiscard]] const BTreeNode *search(const std::string &Id) const;

    void insertNonFull(const Game &game);

    void splitChild(int i, BTreeNode *y);

    void remove(const std::string &k);

    int findKey(const std::string &k);

    void removeFromLeaf(int idx);

    void removeFromNonLeaf(int idx);

    void fill(int idx);

    void merge(int idx);

    void borrowFromPrev(int idx);

    void borrowFromNext(int idx);

    friend class BTree;
};

const BTreeNode *BTreeNode::search(const std::string &Id) const
{
    size_t i = 0;
    while (i < games.size() && Id > games[i].Id)
    {
        i++;
    }
    if (i < games.size() && games[i].Id == Id)
    {
        return this;
    }
    return leaf ? nullptr : children[i]->search(Id);
}

void BTreeNode::splitChild(int i, BTreeNode *y)
{
    auto z = std::make_unique<BTreeNode>(y->leaf, t);
    for (int j = 0; j < t - 1; j++)
    {
        z->games.push_back(y->games[j + t]);
    }
    if (!y->leaf)
    {
        for (int j = 0; j < t; j++)
        {
            z->children.push_back(std::move(y->children[j + t]));
        }
    }
    y->games.erase(y->games.begin() + t, y->games.end());
    children.insert(children.begin() + i + 1, std::move(z));
    games.insert(games.begin() + i, y->games[t - 1]);
}

void BTreeNode::traverse() const
{
    size_t i;
    for (i = 0; i < games.size(); i++)
    {
        if (!leaf)
            children[i]->traverse();
        games[i].display();
        std::cout << "\n";
    }
    if (!leaf && i < children.size())
    {
        children[i]->traverse();
    }
}

void BTreeNode::insertNonFull(const Game &game)
{
    std::vector<Game>::size_type i = games.size();

    if (leaf)
    {
        games.push_back(game);
        std::sort(games.begin(), games.end(), [](const Game &a, const Game &b)
                  { return a.Id < b.Id; });
    }
    else
    {
        while (i > 0 && games[i - 1].Id > game.Id)
        {
            --i;
        }
        if (children[i]->games.size() == 2 * t - 1)
        {
            splitChild(i, children[i].get());
            if (games[i].Id < game.Id)
            {
                ++i;
            }
        }
        children[i]->insertNonFull(game);
    }
}

void BTreeNode::remove(const std::string &k)
{
    int idx = findKey(k);

    if (idx < games.size() && games[idx].Id == k)
    {
        if (leaf)
            removeFromLeaf(idx);
        else
            removeFromNonLeaf(idx);
    }
    else
    {
        if (leaf)
        {
            std::cout << "O game: " << k << " nao existe na arvore\n";
            return;
        }

        bool flag = (idx == games.size());

        if (children[idx]->games.size() < t)
            fill(idx);

        if (flag && idx > games.size())
            children[idx - 1]->remove(k);
        else
            children[idx]->remove(k);
    }
}

int BTreeNode::findKey(const std::string &k)
{
    int idx = 0;
    while (idx < games.size() && games[idx].Id < k)
        ++idx;
    return idx;
}

void BTreeNode::removeFromLeaf(int idx)
{
    games.erase(games.begin() + idx);
}

void BTreeNode::removeFromNonLeaf(int idx)
{
    std::string k = games[idx].Id;

    if (children[idx]->games.size() >= t)
    {
        Game pred = games[idx];
        BTreeNode *child = children[idx].get();
        while (!child->leaf)
            child = child->children.back().get();

        pred = child->games.back();
        child->remove(pred.Id);
        games[idx] = pred;
    }
    else if (children[idx + 1]->games.size() >= t)
    {
        Game succ = games[idx];
        BTreeNode *child = children[idx + 1].get();
        while (!child->leaf)
            child = child->children[0].get();

        succ = child->games[0];
        child->remove(succ.Id);
        games[idx] = succ;
    }
    else
    {
        merge(idx);
        children[idx]->remove(k);
    }
}

void BTreeNode::fill(int idx)
{
    if (idx != 0 && children[idx - 1]->games.size() >= t)
        borrowFromPrev(idx);

    else if (idx != games.size() && children[idx + 1]->games.size() >= t)
        borrowFromNext(idx);

    else
    {
        if (idx != games.size())
            merge(idx);
        else
            merge(idx - 1);
    }
}

void BTreeNode::borrowFromPrev(int idx)
{
    BTreeNode *child = children[idx].get();
    BTreeNode *sibling = children[idx - 1].get();

    for (std::vector<Game>::size_type i = child->games.size(); i-- > 0;)
        child->games[i + 1] = child->games[i];

    if (!child->leaf)
    {
        for (std::vector<Game>::size_type i = child->games.size(); i-- > 0;)
            child->children[i + 1] = std::move(child->children[i]);
    }

    child->games[0] = games[idx - 1];

    if (!leaf)
        child->children[0] = std::move(sibling->children[sibling->games.size()]);

    games[idx - 1] = sibling->games[sibling->games.size() - 1];

    child->games.push_back(games[idx]);
    if (!leaf)
        child->children.push_back(std::move(sibling->children[sibling->games.size()]));

    sibling->games.pop_back();
}

void BTreeNode::borrowFromNext(int idx)
{
    BTreeNode *child = children[idx].get();
    BTreeNode *sibling = children[idx + 1].get();

    child->games.push_back(games[idx]);

    if (!child->leaf)
        child->children.push_back(std::move(sibling->children[0]));

    games[idx] = sibling->games[0];

    for (int i = 1; i < sibling->games.size(); ++i)
        sibling->games[i - 1] = sibling->games[i];

    if (!sibling->leaf)
    {
        for (int i = 1; i <= sibling->games.size(); ++i)
            sibling->children[i - 1] = std::move(sibling->children[i]);
    }

    sibling->games.pop_back();
}

void BTreeNode::merge(int idx)
{
    BTreeNode *child = children[idx].get();
    BTreeNode *sibling = children[idx + 1].get();

    child->games.push_back(games[idx]);

    for (const auto &game : sibling->games)
        child->games.push_back(game);

    if (!child->leaf)
    {
        for (int i = 0; i <= sibling->games.size(); ++i)
            child->children.push_back(std::move(sibling->children[i]));
    }

    for (int i = idx + 1; i < games.size(); ++i)
        games[i - 1] = games[i];

    for (int i = idx + 2; i <= children.size(); ++i)
        children[i - 1] = std::move(children[i]);

    games.pop_back();
    children.pop_back();
}

class BTree
{
    std::unique_ptr<BTreeNode> root;
    int t;

public:
    explicit BTree(int t) : root(nullptr), t(t) {}

    void traverse() const
    {
        if (root)
            root->traverse();
    }

    [[nodiscard]] const Game *search(const std::string &Id) const
    {
        if (root)
        {
            const BTreeNode *result = root->search(Id);
            if (result)
            {
                auto it = std::find_if(result->games.begin(), result->games.end(),
                                       [&Id](const Game &game)
                                       { return game.Id == Id; });
                if (it != result->games.end())
                {
                    return &(*it);
                }
            }
        }
        return nullptr;
    }

    void insert(const Game &game);

    void remove(const std::string &k);

    void edit(const std::string &oldId, const Game &newGame);

    void readGames(const std::string &filename);

    void saveGames(const std::string &filename);

    void saveIds(const std::string &filename);

    void loadGames(const std::string &filename);

private:
    void readGamesHelper(BTreeNode *node, std::vector<std::string> &gameIds)
    {
        if (node)
        {
            for (const auto &game : node->games)
            {
                gameIds.push_back(game.Id);
            }
            for (const auto &child : node->children)
            {
                readGamesHelper(child.get(), gameIds);
            }
        }
    }
};

void BTree::insert(const Game &game)
{
    if (!root)
    {
        root = std::make_unique<BTreeNode>(true, t);
        root->games.push_back(game);
    }
    else
    {
        if (root->games.size() == 2 * t - 1)
        {
            auto s = std::make_unique<BTreeNode>(false, t);
            s->children.push_back(std::move(root));
            s->splitChild(0, s->children[0].get());
            int i = 0;
            if (s->games[0].Id < game.Id)
            {
                i++;
            }
            s->children[i]->insertNonFull(game);
            root = std::move(s);
        }
        else
        {
            root->insertNonFull(game);
        }
    }
}

void BTree::remove(const std::string &k)
{
    if (!root)
    {
        std::cout << "A arvore esta vazia\n";
        return;
    }

    root->remove(k);

    if (root->games.empty())
    {
        std::unique_ptr<BTreeNode> tmp = std::move(root);
        if (root->leaf)
            root.reset(nullptr);
        else
            root = std::move(tmp->children[0]);
    }
}

void BTree::edit(const std::string &oldId, const Game &newGame)
{
    remove(oldId);
    insert(newGame);
}

void BTree::readGames(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Nao conseguiu abrir: " << filename << "\n";
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string name, gender, store;
        std::vector<std::string> stores;

        std::getline(iss, name, ',');
        std::getline(iss, gender, ',');
        while (std::getline(iss, store, ','))
        {
            stores.push_back(store);
        }

        Game game(name, gender, stores);
        insert(game);
    }
}

void BTree::saveGames(const std::string &filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Nao conseguiu abrir: " << filename << "\n";
        return;
    }

    std::function<void(const BTreeNode *)> saveNode = [&](const BTreeNode *node)
    {
        if (node)
        {
            for (const auto &game : node->games)
            {
                file << game.name << "," << game.gender;
                for (const auto &store : game.stores)
                {
                    file << "," << store;
                }
                file << "\n";
            }
            for (const auto &child : node->children)
            {
                saveNode(child.get());
            }
        }
    };

    saveNode(root.get());
}

void BTree::saveIds(const std::string &filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Nao conseguiu abrir: " << filename << "\n";
        return;
    }

    std::vector<std::string> gameIds;
    readGamesHelper(root.get(), gameIds);
    for (const auto &id : gameIds)
    {
        file << id << "\n";
    }
}

void BTree::loadGames(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Nao conseguiu abrir: " << filename << "\n";
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string name, gender, store;
        std::vector<std::string> stores;

        std::getline(iss, name, ',');
        std::getline(iss, gender, ',');
        while (std::getline(iss, store, ','))
        {
            stores.push_back(store);
        }

        Game game(name, gender, stores);
        insert(game);
    }
}

void displayMenu()
{
    std::cout << "Menu:\n";
    std::cout << "1. Insere um Game\n";
    std::cout << "2. Procura um Game\n";
    std::cout << "3. Remove um Game\n";
    std::cout << "4. Edita um Game\n";
    std::cout << "5. Mostrar os Games\n";
    std::cout << "6. Salvar e Sair\n";
    std::cout << "Insira a opcao: ";
}

int main()
{
    BTree tree(3);
    const std::string gamesFilename = "games.txt";

    tree.loadGames(gamesFilename);

    while (true)
    {
        displayMenu();
        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 1)
        {
            system("cls");
            std::string name, gender, store;
            std::vector<std::string> stores;

            std::cout << "Insira o nome: ";
            std::getline(std::cin, name);
            std::cout << "Insira o genero: ";
            std::getline(std::cin, gender);
            std::cout << "Insira as lojas (separadas por virgula): ";
            std::getline(std::cin, store);
            std::istringstream iss(store);
            while (std::getline(iss, store, ','))
            {
                stores.push_back(store);
            }

            tree.insert(Game(name, gender, stores));
        }
        else if (choice == 2)
        {
            system("cls");
            std::string id;
            std::cout << "Insira o ID para busca: ";
            std::getline(std::cin, id);
            const Game *game = tree.search(id);
            if (game)
            {
                game->display();
            }
            else
            {
                std::cout << "Jogo nao encontrado\n";
            }
        }
        else if (choice == 3)
        {
            system("cls");
            std::string id;
            std::cout << "Insira o ID para remover: ";
            std::getline(std::cin, id);
            tree.remove(id);
        }
        else if (choice == 4)
        {
            system("cls");
            std::string oldId, name, gender, store;
            std::vector<std::string> stores;

            std::cout << "Insira o ID para editar: ";
            std::getline(std::cin, oldId);
            std::cout << "Insira o novo nome: ";
            std::getline(std::cin, name);
            std::cout << "Insira o novo genero: ";
            std::getline(std::cin, gender);
            std::cout << "Insira as novas lojas (separadas por virgula): ";
            std::getline(std::cin, store);
            std::istringstream iss(store);
            while (std::getline(iss, store, ','))
            {
                stores.push_back(store);
            }

            tree.edit(oldId, Game(name, gender, stores));
        }
        else if (choice == 5)
        {
            system("cls");
            tree.traverse();
            system("pause");
        }
        else if (choice == 6)
        {
            system("cls");
            tree.saveGames(gamesFilename);
            tree.saveIds("ids.txt");
            break;
        }
        else
        {
            system("cls");
            std::cout << "Escolha Invalida. Tente novamente.\n";
        }
    }

    return 0;
}