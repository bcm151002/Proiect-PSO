#!/bin/bash

echo "Hello World!1"
echo "Hello World!2"
echo "Hello World!3"
echo "Hello World!4"
echo "Hello World!5"
echo "Hello World!6"
echo "Hello World!7"
echo "Hello World!8"
echo "Hello World!9"

# Ia valoarea parametrului 'user' din query string
username=$(echo "$QUERY_STRING" | grep -oP "username=\K[^&]*")

if [ -n "$user" ]; then
    echo "<p>Parametrul 'username' este: $user</p>"
else
    echo "<p>Parametrul 'username' nu a fost furnizat.</p>"
fi