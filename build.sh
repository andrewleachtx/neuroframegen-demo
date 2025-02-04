cmake --build build --parallel

if [[ $? -ne 0 ]]; then
    echo "Failed building"
    exit 1
fi

./build/main