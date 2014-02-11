# SED script to convert a text file into a C string, stripping
# comments and other stuff the compiler doesn't need.

# remove C++ style comments
s/\/\/.*//

# Backslash escape " and \
s/["\\]/\\&/g

# Escape tabs
# NOTE: Can't use \t, because it works only on certain implementations!
s/[	]/\\t/g

# Delete empty lines (Bad idea! Breaks error message line numbers.)
#/^$/d

# Escape newlines
s/$/\\n/

# Begin quote
s/^/\"/

# End quote
s/$/\"/
