#!/bin/bash
INPUT="config/r_config.fsal"
OUTPUT="fsal/config_embedded.h"

if [ ! -f "$INPUT" ]; then
    echo "Error: $INPUT not found"
    exit 1
fi

{
    echo "#ifndef CONFIG_EMBEDDED_H"
    echo "#define CONFIG_EMBEDDED_H"
    echo ""
    echo "static const char EMBEDDED_RECOMMENDED_FSAL[] = "
} > "$OUTPUT"

while IFS= read -r line || [ -n "$line" ]; do
    # Escape backslashes and quotes
    escaped_line=$(echo "$line" | sed 's/\\/\\\\/g' | sed 's/"/\\"/g')
    echo "\"$escaped_line\\n\"" >> "$OUTPUT"
done < "$INPUT"

{
    echo ";"
    echo ""
    echo "#endif"
} >> "$OUTPUT"

echo "Generated $OUTPUT"
