ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <trace(s)>" >&2
    exit 1
fi

FILE="$1"

if [ ! -e ./build/command_line_tool/clltk ]; then
    echo "missing build/command_line_tool/clltk" >&2
    exit 1
fi

n=10000
clean_up() {
    type="$1"
    cp build/output.$type.raw.txt build/output.$type.txt
    sed -i 's/0\{2,\}/0/g' build/output.$type.txt
    sed -i 's/0x0/0/g' build/output.$type.txt
    sed -i 's/  \+/ /g' build/output.$type.txt
    sort -n build/output.$type.txt -o build/output.$type.txt
    tail -n $n build/output.$type.txt >build/output.$type.tail.txt
}

(
    rm build/output.cpp.txt
    if [ ! -e build/output.cpp.raw.txt ]; then
        ./build/command_line_tool/clltk de "$FILE" -o build/output.cpp.raw.txt
    fi
    clean_up cpp
) &
(
    if [ ! -e build/output.python.raw.txt ]; then
        ./decoder_tool/python/clltk_decoder.py -s "$FILE" -o build/output.python.raw.txt
    fi
    clean_up python
) &
wait
diff -u build/output.python.tail.txt build/output.cpp.tail.txt >build/output.tail.diff
