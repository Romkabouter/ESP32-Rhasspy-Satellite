fmt:
	clang-format -i include/*.h include/lwmqtt/*.h -style="{BasedOnStyle: Google, ColumnLimit: 120}"
	clang-format -i src/*.c src/*.h -style="{BasedOnStyle: Google, ColumnLimit: 120}"
	clang-format -i src/os/*.c -style="{BasedOnStyle: Google, ColumnLimit: 120}"
	clang-format -i example/*.c -style="{BasedOnStyle: Google, ColumnLimit: 120}"
	clang-format -i tests/*.cpp -style="{BasedOnStyle: Google, ColumnLimit: 120}"

gtest:
	git clone --depth 1 https://github.com/google/googletest.git gtest
