echo "locks"
g++ locks.cpp -o locks -std=c++17 -fopenmp && time ./locks
echo "actors"
g++ actors.cpp -o actors -std=c++17 -fopenmp && time ./actors
