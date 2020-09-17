###
# Static analysis checking, you'll need to install
# cppcheck for this to work: http://cppcheck.sourceforge.net/
###

cppcheck --enable=all --suppress=missingIncludeSystem .
