# projetoRC
Projeto para a cadeira de Redes e Computadores (RC) 2022/2023

# BOM DIA!

Antes do utilizador se atrever a correr qualquer comando, deverá primeiro executar o comando `make` no diretório do projeto.
Doravante, para correr os comandos basta guiar-se pelo enunciado, visto que a sintaxe é idêntica.

Para alterar os timeouts, no caso do cliente, basta alterar SOCKET_TIMEOUT no início do seu código.
No servidor, a constante SOCKET_TIMEOUT está definida no *header* file acompanhante.
Para remover por completo os timeouts, recomendamos que os coloquem num valor arbitrariamente alto, p. ex, SIZE_MAX.
