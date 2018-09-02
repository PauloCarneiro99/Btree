Algoritmo de insercao e busca em uma arvore B desenvolvido na linguagem C. Para uma melhora de desempenho foi utilizado um pequeno buffer pool, diminuindo assim os acessos a disco, deixando o codigo mais eficiente. Para o buffer pool, foi adotado a politica de substituicao LRU.

### Compilando

---

Para compilar o codigo digite a seguinte linha de comando no terminal : gcc arvoreB.c -o exe

### Executando 

---

Para rodar a insercao em arvoreB digite a seguinte linha de comando no terminal:
	./exe 1 Dados.csv

Para rodar a funcionalidade de busca , que utiliza a arvore B digite a seguinte linha de comando no terminal:
	./exe 12 insiraAquiAChaveBuscada
