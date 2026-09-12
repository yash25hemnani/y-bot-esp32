#include <Arduino.h>
#include <vector>

std::vector<String> split(char sep, String str)
{
    std::vector<String> vec;
    int i = 0;
    String word = "";

    while (str.charAt(i) != '\0')
    {
        while (str.charAt(i) == sep ||
               str.charAt(i) == '\n' ||
               str.charAt(i) == '\r')
        {
            i++;
        }

        while (str.charAt(i) != sep && str.charAt(i) != '\0')
        {
            word.concat(str.charAt(i));
            i++;
        }

        vec.push_back(word);
        word = "";
    }

    return vec;
}