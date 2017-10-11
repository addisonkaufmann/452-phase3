#!/bin/bash

array=($(ls testcases/*.c))
fname="results"
resultsdir="testResults/"
myresultsdir="myResults/"
fext=".txt"
difftext="diff"
maxtest=46
diffdir="diffOutputs/"

rm myResults/* &> /dev/null
rm diffOutputs/* &> /dev/null
mkdir myResults &> /dev/null
mkdir diffOutputs &> /dev/null


count=0

for i in "${array[@]}"
do
   : 

   test="$(cut -d'.' -f1 <<< $i)"
   test="$(cut -d'/' -f2 <<< $test)"
   echo -n "Running $test ....................... "

   make $test &> /dev/null
   eval $test &> $myresultsdir$test$fname$fext
   diffresults="$(diff $resultsdir$test$fext $myresultsdir$test$fname$fext)"
   diffsize=${#diffresults}

   # if [ $test == "test13" ]; then
   #       echo "CHECK DIFF"
   #       diff $resultsdir$test$fext $myresultsdir$test$fname$fext &> $diffdir$test$difftext$fext
   if [ $diffsize -gt 0 ]; then
   		echo "FAILED"
         diff $resultsdir$test$fext $myresultsdir$test$fname$fext &> $diffdir$test$difftext$fext
   else 
   		echo "SUCCEEDED"
   fi

   if [ $count -gt $(expr $maxtest - 1) ]
   then
   		break
   	fi

	count=$(expr $count + 1)

done

make clean &> /dev/null

