#!/bin/bash


# Ia valoarea parametrului 'user' din query string
username=$(echo "$QUERY_STRING" | grep -oP "username=\K[^&]*")

if [ -n "$username" ]; then
    echo "Parametrul 'username' este: $username"
else
    echo "Parametrul 'username' nu a fost furnizat."
fi