# strip trailing whitespace
{
	sub(/[[:space:]]+$/, "")
}

# replace include <sq*.h> by include "../sq*.h"
/include.*\<sq/ {
	a = gensub(/<(.*)>/, "\"../\\1\"", "g");
	print a
	next
}

{
	print $0
}
