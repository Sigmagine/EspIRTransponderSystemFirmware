const minPulse = 300;
const maxPulse = 1800;
const possibilitiesCount = 4;
const pulseCount = 5;
const maxSimilarPulses = 2;
const signalCount = 8;


export class Signal {
    
    constructor(pulses) {
        this.pulses = pulses;
    }

    getDifferenceScore(other){
        
        //check if other can contain this
        if(this.pulses.length > other.pulses.length)return -1;

        //init with the best score we could not even get
        var score = (maxPulse-minPulse)*this.pulses.length; 
        var sameCount = 0;

        //testing all array1 in array2 circular permutations (only those with the same "state" : +=2 )
        for(var sia2 = 0 ; sia2< other.pulses.length; sia2+=2){
            var testScore = 0;
            var testSameCount = 0;
            for(var ia1 = 0 ; ia1< this.pulses.length; ia1++){

                var ia2 = (sia2 + ia1);

                if(ia2 == other.pulses.length){
                    continue; 
                }else if(ia2 > other.pulses.length){
                    ia2 = (ia2-1)%other.pulses.length;
                }
                
                if(this.pulses[ia1] == other.pulses[ia2]){ //check same values count
                    testSameCount++;
                } else { //else compute the absolute difference between the two pulses
                    testScore += Math.abs(this.pulses[ia1]-other.pulses[ia2]);
                }
            }
            if(testScore<score){ //ensure we keep only the worst difference score of all permutations
                score = testScore;
            }
            if(testSameCount>sameCount){ //ensure we keep only the worst same pulses count of all permutations
                sameCount = testSameCount;
            }
        }
        if(sameCount>maxSimilarPulses)return -sameCount; //reject if same pulses count is too high
        return score;

    }


}



export class SignalGroup {

    constructor() {
        this.signals = [];
        this.averageDifferenceScore = null;
        this.worstScore = null;
    }

    push(signal,score){
        this.signals.push(signal);
        this.averageDifferenceScore = null;
        if(this.worstScore === null || score< this.worstScore)this.worstScore = score;
    }

    getAverageDifferrenceScore(){

        if(this.averageDifferenceScore === null){

            if(this.signals.length>1){

                var count = 0;
                var sum = 0;
                var str = "";
                
                for(var i = 0; i< this.signals.length-1; i++){
                    for(var j = i+1; j< this.signals.length; j++){
                        sum += this.signals[i].getDifferenceScore(this.signals[j]);
                        count ++;
                    }
                }

                this.averageDifferenceScore = Math.round(sum/count);

            }else{
                this.averageDifferenceScore = -1;
            }
        }

        return this.averageDifferenceScore;
    }


}

export class IrTransponder {

    constructor(container) {

        this.rawPulses = [];

        this.pulseDiff = (maxPulse-minPulse)/(possibilitiesCount*2);
        for(var i = 0 ; i< possibilitiesCount; i++){
            this.rawPulses.push(Math.round(minPulse+this.pulseDiff*(2*i+1)));
        }

        this.table = document.createElement('table');

        this.signals = [];
        this.generate(pulseCount,[]);

        var bestWorstScore = 0;
        var bestAVGScore = 0;
        var bestWorstScoreSignal = null;
        var bestAVGScoreSignal = null;

        for(var i = 0 ; i< this.signals.length; i++){

            var signalGroup = this.findBestDifferent(signalCount,[i]);

            /*var tr = document.createElement('tr');
            this.table.appendChild(tr);
            
            var td = document.createElement('td');
            td.innerText = "{ "+this.signals[i].pulses.join(" , ")+" }";
            tr.appendChild(td);

            

            var td = document.createElement('td');
            td.innerText = signalGroup.getAverageDifferrenceScore();
            tr.appendChild(td);

            var td = document.createElement('td');
            td.innerText = JSON.stringify(signalGroup);
            tr.appendChild(td);*/

            if(signalGroup.worstScore > bestWorstScore){
                bestWorstScore = signalGroup.worstScore;
                bestWorstScoreSignal = signalGroup;
            }

            if(signalGroup.getAverageDifferrenceScore() > bestAVGScore){
                bestAVGScore = signalGroup.getAverageDifferrenceScore();
                bestAVGScoreSignal = signalGroup;
            }
        }
        container.appendChild(this.table);

        var td = document.createElement('div');
        td.innerHTML += "BestWorstScore :<br/>";
        td.innerHTML += JSON.stringify(bestWorstScoreSignal)+"<br/>";
        td.innerHTML += "BestAVGScore :<br/>";
        td.innerHTML += JSON.stringify(bestAVGScoreSignal)+"<br/>";


        container.appendChild(td);
    }

    findBestDifferent(k,a){
        if(k==0){
            return new SignalGroup();
        }else{
            var score = 0;
            var index = -1;
            for(var i = 0 ; i< this.signals.length; i++){
                var areBetter = true;
                var groupMinScore = 500000;
                for(var j = 0 ; j< a.length; j++){
                    var test = this.signals[a[j]].getDifferenceScore(this.signals[i]);
                    if(test <= score){
                        areBetter = false;
                        break;
                    }else if(test < groupMinScore){
                        groupMinScore = test;
                    }
                }
                if(areBetter){
                    score = groupMinScore;
                    index = i;
                }
            }

            if(index>=0){
                a.push(index);
                var bests = this.findBestDifferent(k-1,a);
                bests.push(this.signals[index],score);
                return bests;
            }else{
                return new SignalGroup();
            }

        }
        
    }

    

    generate(k,a){
        if(k==0){
            this.signals.push(new Signal(a));
        }else{
            for(var i = 0 ; i< possibilitiesCount; i++){
                var a2 = a.slice();
                a2.push(this.rawPulses[i]);
                this.generate(k-1,a2);
            }
        }
    }

}


let tableContainer = document.getElementById('table');

let irTransponder = new IrTransponder(tableContainer);













