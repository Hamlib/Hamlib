#!/bin/sh

# Reject cache implementation access outside its owning source file.

set -eu

test_dir=${srcdir:-$(dirname "$0")}

case $test_dir in
/*)
    ;;
*)
    test_dir=$(cd "$test_dir" && pwd)
    ;;
esac

source_root=$(cd "$test_dir/.." && pwd)
file_list=$(mktemp "${TMPDIR:-/tmp}/hamlib-routing-access.XXXXXX")
trap 'rm -f "$file_list"' EXIT HUP INT TERM

for directory in src include rigs rotators amplifiers tests test
do
    if test -d "$source_root/$directory"
    then
        find "$source_root/$directory" -type f \
            \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \
               -o -name '*.h' \) -print >> "$file_list"
    fi
done

failure=0

while IFS= read -r file
do
    case $file in
    "$source_root/src/cache.c"|"$source_root/src/multicast.c")
        continue
        ;;
    esac

    if ! awk '
        BEGIN {
            direct_cache = "CACHE[[:space:]]*\\("
            named_pointer = "(cache_addr|cachep)[[:space:]]*" \
                "->[[:space:]]*[A-Za-z_]"
            parenthesized_pointer = "\\([[:space:]]*(cache_addr|cachep)" \
                "[[:space:]]*\\)[[:space:]]*->[[:space:]]*[A-Za-z_]"
            cache_size = "sizeof[[:space:]]*\\([[:space:]]*struct" \
                "[[:space:]]+rig_cache([[:space:]]|\\*)"
            cache_pointer = "struct[[:space:]]+rig_cache[[:space:]]*\\*"
            routing_fields = "(current_vfo|rx_vfo|tx_vfo)"
            state_accessor = "(STATE|HAMLIB_STATE)[[:space:]]*" \
                "\\([^)]*\\)[[:space:]]*" \
                "->[[:space:]]*(current_vfo|rx_vfo|tx_vfo)"
            state_addr = "state_addr[[:space:]]*->[[:space:]]*" \
                routing_fields
        }

        function sanitize(line,    c, i, next_c, out, quote)
        {
            out = ""
            quote = string_quote
            escaped = 0

            for (i = 1; i <= length(line); i++)
            {
                c = substr(line, i, 1)
                next_c = substr(line, i + 1, 1)

                if (in_block_comment)
                {
                    if (c == "*" && next_c == "/")
                    {
                        in_block_comment = 0
                        out = out "  "
                        i++
                    }
                    else
                    {
                        out = out " "
                    }

                    continue
                }

                if (quote != "")
                {
                    out = out " "

                    if (escaped)
                    {
                        escaped = 0
                    }
                    else if (c == "\\")
                    {
                        escaped = 1
                    }
                    else if (c == quote)
                    {
                        quote = ""
                    }

                    continue
                }

                if (c == "/" && next_c == "*")
                {
                    in_block_comment = 1
                    out = out "  "
                    i++
                }
                else if (c == "/" && next_c == "/")
                {
                    break
                }
                else if (c == "\"" || c == sprintf("%c", 39))
                {
                    quote = c
                    out = out " "
                }
                else
                {
                    out = out c
                }
            }

            string_quote = quote
            return out
        }

        function parent_is_skipped()
        {
            return conditional_depth > 1 && skipped[conditional_depth - 1]
        }

        function remember_state_aliases(text,    declaration, name)
        {
            while (match(text, "(const[[:space:]]+)?struct[[:space:]]+" \
                    "rig_state[[:space:]]*\\*[[:space:]]*" \
                    "[A-Za-z_][A-Za-z0-9_]*"))
            {
                declaration = substr(text, RSTART, RLENGTH)
                sub(/^.*\*[[:space:]]*/, "", declaration)
                name = declaration
                sub(/[^A-Za-z0-9_].*$/, "", name)
                state_aliases[name] = 1
                text = substr(text, RSTART + RLENGTH)
            }
        }

        function has_state_alias_access(text,    name, pattern)
        {
            for (name in state_aliases)
            {
                pattern = "(^|[^A-Za-z0-9_])" name \
                    "[[:space:]]*->[[:space:]]*" routing_fields

                if (text ~ pattern)
                {
                    return 1
                }
            }

            return 0
        }

        {
            clean = sanitize($0)

            if (clean ~ /^[[:space:]]*#[[:space:]]*if(def|ndef)?([[:space:]]|$)/)
            {
                conditional_depth++
                zero_branch[conditional_depth] = \
                    clean ~ /^[[:space:]]*#[[:space:]]*if[[:space:]]+0([[:space:]]|$)/
                skipped[conditional_depth] = parent_is_skipped() \
                    || zero_branch[conditional_depth]
                next
            }

            if (clean ~ /^[[:space:]]*#[[:space:]]*(else|elif)([[:space:]]|$)/)
            {
                if (conditional_depth > 0 && zero_branch[conditional_depth] \
                        && !parent_is_skipped())
                {
                    zero_branch[conditional_depth] = 0
                    skipped[conditional_depth] = 0
                }

                next
            }

            if (clean ~ /^[[:space:]]*#[[:space:]]*endif([[:space:]]|$)/)
            {
                delete skipped[conditional_depth]
                delete zero_branch[conditional_depth]

                if (conditional_depth > 0)
                {
                    conditional_depth--
                }

                next
            }

            if (conditional_depth > 0 && skipped[conditional_depth])
            {
                next
            }

            candidate = previous_two_lines " " previous_line " " clean
            remember_state_aliases(candidate)

            if (candidate ~ direct_cache || candidate ~ named_pointer \
                    || candidate ~ parenthesized_pointer \
                    || candidate ~ cache_size || candidate ~ state_accessor \
                    || candidate ~ state_addr \
                    || has_state_alias_access(candidate) \
                    || (FILENAME ~ /\\.(c|cc|cpp)$/ \
                        && candidate ~ cache_pointer))
            {
                print "direct rig routing/cache access outside src/cache.c: " \
                    FILENAME \
                    > "/dev/stderr"
                exit 1
            }

            previous_two_lines = previous_line
            previous_line = clean
        }
    ' "$file"
    then
        failure=1
    fi
done < "$file_list"

exit "$failure"
