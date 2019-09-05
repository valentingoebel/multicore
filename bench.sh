g++ -O3 locks.cpp -o locks -std=c++17 -fopenmp
g++ -O3 actors.cpp -o actors -std=c++17 -fopenmp

echo "locks"
time ./locks
echo "actors"
time ./actors
