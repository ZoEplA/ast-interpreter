#!/bin/bash      
for((i=0;i<=9;i++));   
do
var1=`cat ./classtest/test0$i.c` 
var2=`cat ./test/test0$i.c`
echo $i   
./ast-interpreter "$var1"
echo "test"
./ast-interpreter "$var2"
done 

for((i=10;i<=24;i++));                                                                                
do                                                                                                  
var1=`cat ./classtest/test$i.c`
echo $i                                                                                             
./ast-interpreter "$var1"                                                                           
done

for((i=10;i<=19;i++));
do
var2=`cat ./test/test$i.c`
echo $i
echo "test"
./ast-interpreter "$var2"
done  
