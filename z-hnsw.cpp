#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cmath>
#include <random>
#include <limits>
#include <algorithm>
#include <cassert>

// Define the Point structure
struct Point {
    std::vector<float> coordinates;
    std::string label;

    Point(std::initializer_list<float> coords, std::string lbl) : coordinates(coords), label(lbl) {}

    // Custom comparator for sorting points based on coordinates
    bool operator<(const Point& other) const {
        return std::lexicographical_compare(coordinates.begin(), coordinates.end(),
                                            other.coordinates.begin(), other.coordinates.end());
    }

    // Equality operator to compare two points
    bool operator==(const Point& other) const {
        return coordinates == other.coordinates && label == other.label;
    }
};

// Calculate the Euclidean distance between two points
float euclideanDistance(const Point& p1, const Point& p2) {
    float distance = 0.0f;
    for (size_t i = 0; i < p1.coordinates.size(); ++i) {
        distance += (p1.coordinates[i] - p2.coordinates[i]) * (p1.coordinates[i] - p2.coordinates[i]);
    }
    return std::sqrt(distance);
}

// Define the Node structure
struct Node {
    Point point;
    std::vector<std::vector<Node*>> neighbors;  // Neighbors at different levels

    Node(const Point& pt, int maxLevel) : point(pt), neighbors(maxLevel + 1) {}
};

// Define the Graph structure
class Graph {
public:
    std::vector<Node*> nodes;

    ~Graph() {
        for (Node* node : nodes) {
            delete node;
        }
    }
};

// Define the HNSW structure
class HNSW {
private:
    Graph graph;
    int maxLevel;
    int maxNeighbors; // Maximum number of neighbors
    int efConstruction;  // Size of dynamic list for construction
    float levelMult;
    std::default_random_engine generator;

    int getRandomLevel() {
        std::uniform_real_distribution<float> distribution(0.0, 1.0);
        float r = distribution(generator);
        return static_cast<int>(-std::log(r) * levelMult);
    }

    void insertNode(Node* newNode) {
        if (graph.nodes.empty()) {
            graph.nodes.push_back(newNode);
            return;
        }

        Node* enterPoint = graph.nodes[0];
        int level = maxLevel;

        // Search for the closest neighbor at the top level
        while (level > static_cast<int>(newNode->neighbors.size()) - 1) {
            enterPoint = searchLayer(newNode->point, enterPoint, level, 1).top().second;
            --level;
        }

        // Insert the new node at each level
        for (int l = level; l >= 0; --l) {
            auto topCandidates = searchLayer(newNode->point, enterPoint, l, efConstruction);
            selectNeighbors(newNode, topCandidates, l);
        }

        // Update maxLevel if necessary
        if (static_cast<int>(newNode->neighbors.size()) - 1 > maxLevel) {
            maxLevel = static_cast<int>(newNode->neighbors.size()) - 1;
        }

        graph.nodes.push_back(newNode);
    }


    std::priority_queue<std::pair<float, Node*>> searchLayer(const Point& point, Node* enterPoint, int level, int ef) {
        std::priority_queue<std::pair<float, Node*>> topCandidates;
        std::priority_queue<std::pair<float, Node*>, std::vector<std::pair<float, Node*>>, std::greater<>> candidates;
        std::unordered_map<Node*, bool> visited;

        float lowerBound = euclideanDistance(point, enterPoint->point);
        candidates.emplace(lowerBound, enterPoint);
        topCandidates.emplace(lowerBound, enterPoint);
        visited[enterPoint] = true;

        while (!candidates.empty()) {
            auto currPair = candidates.top();
            float dist = currPair.first;
            Node* currNode = currPair.second;
            candidates.pop();

            if (dist > lowerBound) break;

            for (Node* neighbor : currNode->neighbors[level]) {
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    float d = euclideanDistance(point, neighbor->point);
                    if (topCandidates.size() < static_cast<size_t>(ef) || d < lowerBound) {
                        candidates.emplace(d, neighbor);
                        topCandidates.emplace(d, neighbor);
                        if (topCandidates.size() > static_cast<size_t>(ef)) {
                            topCandidates.pop();
                            lowerBound = topCandidates.top().first;
                        }
                    }
                }
            }
        }

        return topCandidates;
    }

    void selectNeighbors(Node* newNode, std::priority_queue<std::pair<float, Node*>>& topCandidates, int level) {
        std::vector<Node*> neighbors;
        std::unordered_set<Node*> selected;
        while (!topCandidates.empty() && neighbors.size() < static_cast<size_t>(maxNeighbors)) {
            Node* neighbor = topCandidates.top().second;
            topCandidates.pop();
            if (selected.find(neighbor) == selected.end()) {
                neighbors.push_back(neighbor);
                selected.insert(neighbor);
            }
        }
        for (Node* neighbor : neighbors) {
            newNode->neighbors[level].push_back(neighbor);
            neighbor->neighbors[level].push_back(newNode);
        }
    }


public:
    HNSW(int maxNeighbors, int efConstr, float mult) 
        : maxLevel(0), maxNeighbors(maxNeighbors), efConstruction(efConstr), levelMult(mult) {}

    void insert(const Point& point) {
        int level = getRandomLevel();
        Node* newNode = new Node(point, level);
        insertNode(newNode);
    }

    std::vector<Point> search(const Point& query, int k) {
        if (graph.nodes.empty()) return {};

        Node* enterPoint = graph.nodes[0];
        int level = maxLevel;

        // Search at the top level
        while (level > 0) {
            enterPoint = searchLayer(query, enterPoint, level, 1).top().second;
            --level;
        }

        // Search at the lowest level
        auto topCandidates = searchLayer(query, enterPoint, 0, k);

        std::vector<Point> results;
        while (!topCandidates.empty()) {
            results.push_back(topCandidates.top().second->point);
            topCandidates.pop();
        }

        return results;
    }

    void printIndex() const {
        for (const Node* node : graph.nodes) {
            std::cout << "Node(" << node->point.label << ": ";
            for (size_t i = 0; i < node->point.coordinates.size(); ++i) {
                std::cout << node->point.coordinates[i];
                if (i < node->point.coordinates.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << ") -> Levels: " << node->neighbors.size() - 1 << "\n";
            for (size_t level = 0; level < node->neighbors.size(); ++level) {
                std::cout << "  Level " << level << " neighbors: ";
                for (const Node* neighbor : node->neighbors[level]) {
                    std::cout << neighbor->point.label << " ";
                }
                std::cout << "\n";
            }
        }
    }
};

bool verifyNearestNeighbors(const Point& queryPoint, std::vector<Point>& results, const std::vector<Point>& allPoints, size_t k) {
    // Create a priority queue to find the k nearest neighbors in the original data
    std::priority_queue<std::pair<float, Point>> pq;

    for (const Point& point : allPoints) {
        float distance = euclideanDistance(queryPoint, point);
        pq.push(std::make_pair(distance, point));
        if (pq.size() > k) {
            pq.pop();
        }
    }

    // Extract the k nearest neighbors from the priority queue
    std::vector<std::pair<float, Point>> expectedResults;
    while (!pq.empty()) {
        expectedResults.push_back(pq.top());
        pq.pop();
    }

    // Sort the expected results and the actual results by distance to compare them
    auto distanceComparator = [](const std::pair<float, Point>& a, const std::pair<float, Point>& b) {
        return a.first < b.first;
    };

    std::sort(expectedResults.begin(), expectedResults.end(), distanceComparator);

    std::vector<std::pair<float, Point>> actualResults;
    for (const Point& point : results) {
        actualResults.push_back(std::make_pair(euclideanDistance(queryPoint, point), point));
    }
    std::sort(actualResults.begin(), actualResults.end(), distanceComparator);

    // Verify that the actual results match the expected results
    bool isCorrect = (expectedResults == actualResults);

    std::cout << "Expected results:\n";
    for (const auto& pair : expectedResults) {
        const Point& point = pair.second;
        std::cout << point.label << " (";
        for (size_t i = 0; i < point.coordinates.size(); ++i) {
            std::cout << point.coordinates[i];
            if (i < point.coordinates.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << ") Distance: " << pair.first << "\n";
    }

    std::cout << "Actual results:\n";
    for (const auto& pair : actualResults) {
        const Point& point = pair.second;
        std::cout << point.label << " (";
        for (size_t i = 0; i < point.coordinates.size(); ++i) {
            std::cout << point.coordinates[i];
            if (i < point.coordinates.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << ") Distance: " << pair.first << "\n";
    }
    

    return isCorrect;
}

int main() {
    HNSW hnsw(4, 200, 1.0f);

    // Insert some points with 4 coordinates and labels
    std::vector<Point> points = {
        {{1.0, 2.0, 3.0, 4.0}, "A"},
        {{5.0, 6.0, 7.0, 8.0}, "B"},
        {{9.0, 10.0, 11.0, 12.0}, "C"},
        {{13.0, 14.0, 15.0, 21.0}, "D"},
        {{17.0, 18.0, 19.0, 20.0}, "E"},
        {{21.0, 22.0, 23.0, 32.0}, "F"},
        {{25.0, 26.0, 27.0, 28.0}, "G"},
        {{29.0, 30.0, 31.0, 32.0}, "H"},
        {{33.0, 34.0, 35.0, 36.0}, "I"},
        {{37.0, 38.0, 39.0, 40.0}, "J"}
    };

    for (const Point& point : points) {
        hnsw.insert(point);
    }

    hnsw.printIndex();

    // Query for the nearest neighbors
    Point queryPoint = {{15.0, 16.0, 17.0, 18.0}, "Query"};
    std::vector<Point> results = hnsw.search(queryPoint, 3);

    std::cout << "Nearest neighbors to (" << queryPoint.label << ": ";
    for (size_t i = 0; i < queryPoint.coordinates.size(); ++i) {
        std::cout << queryPoint.coordinates[i];
        if (i < queryPoint.coordinates.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "):" << std::endl;
    for (const Point& result : results) {
        std::cout << result.label << " (";
        for (size_t i = 0; i < result.coordinates.size(); ++i) {
            std::cout << result.coordinates[i];
            if (i < result.coordinates.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << ")" << std::endl;
    }

    // Verify the nearest neighbors
    bool isCorrect = verifyNearestNeighbors(queryPoint, results, points, 3);
    std::cout << "Verification result: " << (isCorrect ? "Correct" : "Incorrect") << std::endl;

    return 0;
}