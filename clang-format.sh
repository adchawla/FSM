find -f ./tests -f ./include | grep -e ".*\.\h$" -e ".*\.\cpp$" | xargs clang-format -i