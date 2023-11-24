<!-- LTeX: language=pt-BR -->

# PAGINADOR DE MEMÓRIA -- RELATÓRIO

1. Termo de compromisso

   Ao entregar este documento preenchiso, os membros do grupo afirmam que todo o código desenvolvido para este trabalho é de autoria própria. Exceto pelo material listado no item 3 deste relatório, os membros do grupo afirmam não ter copiado material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

   Preencha as linhas abaixo com o nome e o email dos integrantes do grupo. Substitua marcadores `XX` pela contribuição de cada membro do grupo no desenvolvimento do trabalho (os valores devem somar 100%).

   - Carlos Henrique Brito Malta Leão <chbmleao@gmail.com> 50%
   - Vinícius Alves de Faria Resende <viniciusfariaresende@gmail.com> 50%

3. Referências bibliográficas

   1. Slides da Disciplina de Sistemas Operacionais
   2. The Open Group Base - <https://pubs.opengroup.org/onlinepubs/009604499/functions/pthread_mutex_lock.html>

4. Detalhes de implementação

   1. Descreva e justifique as estruturas de dados utilizadas em sua solução.
        A grande maioria do projeto foi desenvolvido utilizando apenas estruturas básicas do C, como structs e vetores,
    contudo, foi implementada uma Lista Encadeada (Chained List), com implementação unicamente encadeada simples. Apenas 
    métodos básicos foram implementados, como criação, inserção no final, busca e remoção.
        A utilização da estrutura se justifica pelo fato da característica variável do número de processos, nesse contexto,
    a estrutura de lista possuí características que nos ajudam a lidar com um número dinâmico de elementos, além de não
    apresentar uma implementação complexa. Portanto, usamos a lista encadeada para guardar os diferentes processos, onde
    cada nó da lista apresenta um processo que contém sua própria tabela de páginas, e cada célula da tabela tem informações
    pertinentes como o tipo de permissão, endereço virtual, se está presente na memória, se foi acessada recentemente, entre
    outros.

   2. Descreva o mecanismo utilizado para controle de acesso e modificação às páginas.
        Para a implementação do pager a função pager_fault é a que têm o grande papel chave, uma vez que ele é o responsável
    por lidar com as adversidades encontradas durante a execução. Dessa forma, quando um page_fault ocorre, usamos o pid
    fornecido para encontrar o processo em questão na lista de páginas, depois usamos o endereço virtual da página para
    iterar sobre a memória virtual do processo (page_table) para encontrar a célula específica que gerou a falha de página.
        Nesse contexto, tendo a célula que gerou a falha, podemos utilizar as meta-informações da célula para tratar o 
    problema da melhor forma. Por exemplo, se a célula acessada não for válida, sabemos que é um novo acesso e devemos
    alocar espaço para a página. Primeiro verificamos se existem quadros livres na memória, caso positivo, apenas iteramos
    sobre o vetor de quadros e encontramos um vazio, atribuindo-o à pagina. Caso não existam quadros livres, chamamos a 
    função auxliar _handleSwap que é responsável por encontrar um quadro apropriado na memória, enviá-lo para o disco, e
    atribuir o quadro liberado a nova página, além disso lida com outros detalhes como escrever o dado na memória caso 
    exista algum no quadro, executar o algorítimo de segunda chance alterando a nível de proteção da página entre outros.
    Por fim desse processo de nova página, também a setamos como válida e presente.
        Outro caso, é quando a página é valida e presente na memória, nesse caso, sabemos que o problema está relacionado
    a uma falha de permissão de acesso. Caso o processo tenha permissão de PROT_NONE, atribuímos uma de Read-Only, caso
    o processo já tenha uma permissão de Read-Only, atribuímos uma de Read-Write. Esse é o melhor momento para saber que
    uma página foi acessada, portanto, marcamos a página como recentemente acessada para o melhor funcionamento do algo-
    ritmo de segunda chance.
        O terceiro caso ocorre quando o acesso é a um endereço válido mas que não está presente, nesse caso estamos ten-
    tando acessar uma página que foi colocada no disco, e precisamos recuperá-la. O processo é muito similar a quando alo-
    camos uma nova página quando não há quadros disponíveis, inclusive, a mesma função _handleSwap é chamada e lida com 
    todos os detalhes mencionados anteriormente. Por fim a página é marcada como presente novamente.
        Ao final disso tudo, precisamos liberar o mutex que é travado no início do pager_fault para permitir execuções 
    paralelas, é importante tomar cuidado para não terminar a função sem liberá-lo e ocasionar um Deadlock. Existem outras
    funções que atuam no pager.c, como para criar novos processos, removê-los ou até extender a memória de um processo,
    porém, no que tange ao controle de acesso e modificação das páginas, o pager_fault é o que tem o papel principal e 
    foi propriamente descrito em detalhes.
