
Vivien BERNARD-NICOD, Pol-Elouan BRIENT
README- BE_reseau

Commandes de compilation{
    Les commandes de compilations sont les mêmes qu'avec le make
}


Ce qui marche / ne marche pas{
    Les différentes versions fonctionnent avec la fiabilité (stop&wait), la reprise des pertes et établissement de connexion et la négotiation de pertes admissibles. 
} 


Choix d'implémentation{
    stop/wait : utilisation d'un mutex afin de lock ou delock au besoin
    negotiations pertes :   Le puits et la source ont chacun un taux de pertes attitré. Lors de la connexion, le puits va renvoyer son taux de perte acceptable dans son SYNAC,
                            la source va le récuperer et définir le taux de perte avec un min des deux
    asynchronisme :   Nous avons un buffer circulaire de taille 100. Si un message n'est pas acquité on met 1 a sa valeur dans le buffer circulaire, 
                      la notant comme une perte. On récupère alors toutes les pertes si il y a autant ou plus de 1 que le nombre de pertes autorisées.
}

Benefices MICTCP-v4 par rapport a TCP et MICTCP-v2{
    v2 : La v4 nous apporte une garantie de fiablitié grâce au stop&wait, elle gère aussi l'asynchronisme entre le côté serveur et récepteur, ce qui n'est pas présent dans la V2. 
    TCP : Nous avons un gain de performances grâce à la récupération (que TCP ne gère pas -> désavantage pour lui si le réseau est buité) des pertes tout en gardant les
    avantages de TCP (ordre et fiabilité).
}

