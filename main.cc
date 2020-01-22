#include <iostream>
#include <cmath>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <cstdlib>
#include <stack>
#include <ctime>
#include <vector>
#include <bits/stdc++.h>
#include <csignal>

using namespace std;

#define MIN_ROUNDS 1
#define MAX_ROUNDS 13 //change this for different speeds

#define MIN_PLAYERS 2
#define MAX_PLAYERS 4 //change this for different speeds

#define MIN_GAMES 1

#define WIN_FACT 1 //factor to multiply with
#define LOSE_FACT 1
#define WIN_CONST 10

#define MAX_CARDS MAX_ROUNDS*MAX_PLAYERS

#define CARD_DIM 2

#define MCTS_CONSTANT sqrt(2)

int numberOfPlayers;
int numberOfRounds;
int numberOfGames;

int numberOfCards;
int numberOfSuits;
int orderSize;

int mcBudget;

int currentTrump = -1; //suit that is trump
bool suitPlayer[MAX_PLAYERS][MAX_PLAYERS]; //reducing the flexibility of number of suits here!!!!
										   //if suitPlayer[i][j] == true, suit i no longer in possesion of player j

int agentSelection[MAX_PLAYERS];
int algSelection[MAX_PLAYERS][2];

clock_t start;
double duration;

int totalPoints = 0;
int totalPPoints[MAX_PLAYERS] = {};
int totalPwins[MAX_PLAYERS] = {};
int absoluteVictorCounter[MAX_PLAYERS] = {};

int tempGames = 0;
int leafQuit = 0;
int simulationsCounter = 0;

string suitNames[MAX_PLAYERS];
string orderNames[MAX_ROUNDS];
string agentNames[5];
int agentSettings[5][2];

struct mctsNode {
	//int player; //which player is at to move at root?
	mctsNode * parent;
	//The arrays below define WHICH MOVE has just been played, and by WHOM
	int curTrick[MAX_PLAYERS][CARD_DIM]; //Which cards are currently ``on the table''?
	int trickOrder[MAX_PLAYERS]; //what is the current trick order?
	int player;

	//More global game state info
	bool blockedSuits[MAX_PLAYERS][MAX_PLAYERS]; //which suits are blocked for other players due to previous moves?
	int cardInfo[MAX_PLAYERS][MAX_ROUNDS]; //which card belongs to the player/which card has been played already?
	int tricksMade[MAX_PLAYERS]; //who has won how many tricks?

	//node info
	double evaluations; //evaluation so far
	int simulations; //number of simulation ran

	//node expansion
	int numberOfChildren;
	int unexploredMoves[MAX_CARDS][CARD_DIM]; //possible moves that haven't been checked yet;
	mctsNode * children[MAX_CARDS];

	mctsNode(mctsNode * p, int nodePlayer){
		player = nodePlayer;
		parent = p;
		evaluations = 0;
		simulations = 0;
		numberOfChildren = 0;
		if(parent != NULL){
			parent->children[parent->numberOfChildren] = this;
			parent->numberOfChildren++;
		}

		for(int i = 0; i < MAX_CARDS; i++){
			children[i] = NULL;
			unexploredMoves[i][0] = -1;
			unexploredMoves[i][1] = -1;
		}
	}
};

//Call back handler
void signalHandler(int signum) {
	cout << "Caught signal " << signum << endl;
	duration = ( clock() - start ) / (double) CLOCKS_PER_SEC;
	cout << "Total scoreboard points(" << tempGames << " games): "<< totalPoints << endl;;
	for(int i = 0; i < numberOfPlayers; i++){
		cout << "Player " << i << "(" << agentNames[agentSelection[i]] << "): " << endl;
		cout << "Total points: " << totalPPoints[i] << endl;
		cout << "Player share: " << (double)totalPPoints[i]/totalPoints *100.0  << "%" << endl;
		cout << "Player wins: " << (double)totalPwins[i]/tempGames * 100.0 << "%" << endl;
		cout << "Number of absolute victories: " << absoluteVictorCounter[i] << endl;
	}

	cout << "Duration of calculation: " << (int)(duration/3600) << "h " << ((int)duration%3600) << "m " << ((int)duration%60) << "s" << endl;
	//cout << "Leaf quits: " << leafQuit << "/" << simulationsCounter << endl;
   // Terminate program
   exit(signum);
}

/********************************************
 * PRINT Functions
 * Functions to call to display information in the terminal
 *
 */

void printAllCards (int cards[][MAX_ROUNDS]){ //prints all cards their ownership
    for(int i = 0; i < numberOfSuits; i++){
        for (int j = 0; j < numberOfRounds; j++){
            cout << cards[i][j] << " ";
        }
        cout << endl;
    }
}

void printOrder(int order[]){
	cout << endl;
	for(int i = 0; i < numberOfPlayers; i++){
		cout << i << ": Player #" << order[i] << endl;
	}
	cout << endl;
}

void printCards(int cards[][CARD_DIM], int length){ //prints some 2d array containing cards
	for (int i = 0; i < length; i++){
		cout << "Suit: " << cards[i][0] << ", Order: " << cards[i][1] << endl;
	}
}

void printRoundScore(int curRound, int bids[], int wins[]){
	cout << endl << endl << "--- Scores of round " << curRound << " ---" << endl << endl;
	for (int i = 0; i < numberOfPlayers; i++){
		cout << "Player #" << i << " bid " << bids[i] << " and won " << wins[i] << " tricks." << endl;
	}
	cout << endl;
}

void printScoreBoard(int score[]){
	cout << endl << endl << "--- Score Board ---" << endl << endl;
	for (int i = 0; i < numberOfPlayers; i++){
		cout << "Player #" << i << " has " << score[i] << " points." << endl;
	}
	cout << endl;
}

/******************************************************************************
 * EVALUATION Functions:
 * These functions assist the agents in bidding based on their hand
 * Or evaluate other statistics about the game at the current point
 */

int evaluateRandomBid(int maxBid, int iBid){
	int bid = rand() % maxBid+1;
	while(bid == iBid){
		bid = rand() % (maxBid+1);
	}
	return bid;
}

double evalCardCurrent(int cards[][MAX_ROUNDS], int currentTrick[][CARD_DIM], int player, int cSuit, int cOrder){
    double eval = 0.0;
    int nPlayedCards = 0;
    for(int i = 0; i < numberOfPlayers; i++){
        if(currentTrick[i][0] == -1){
            break;
        }
        nPlayedCards++;
    }

	int trumpOrder = 0;
    int oPlayed = 0;
    for (int i = cOrder; i < orderSize; i++){
        if(cards[cSuit][i] == -2 || cards[cSuit][i] == player){ //a suit card was played at some point or is owned by the player
            oPlayed++;
        }
    }
    if(currentTrump != -1 && cSuit != currentTrump){ //card isnt a trump card, add factor
        trumpOrder = orderSize;
        for(int i = 0; i < orderSize; i++){
            if(cards[currentTrump][i] == -2 ||
				cards[currentTrump][i] == player){ //trump card has been played or is owned by the player
                trumpOrder--;
            }
        }
    }

	eval = 1.0 - (double)((abs(cOrder - orderSize) - oPlayed) + trumpOrder) /((orderSize - oPlayed) + trumpOrder);

    if(nPlayedCards > 0){ //A card has already been played in this trick, check if this card is even still usefull
        if(currentTrick[0][0] != cSuit){
            if(cSuit == currentTrump){ //checking a trump card
                for(int i = 1; i < nPlayedCards; i++){
                    if(currentTrick[i][0] == currentTrump){ //checking for more trumps
                        if(currentTrick[i][1] > cOrder){
                            eval = eval * -1.0;
							break;
                        }
                    }
                }
            }
            else { //card has no chance of winning
                eval = eval * -1.0;
            }
        }
        else if(currentTrick[0][0] == cSuit){ //matched the suit
            for(int i = 0; i < nPlayedCards; i++){ //check if other matchers are better or if there are trumps
                if(currentTrick[i][0] == currentTrump ||
                    (currentTrick[i][0] == currentTrick[0][0] && currentTrick[i][1] > cOrder)){
                    eval = eval * -1.0;
					break;
                }
            }
        }
    }
	return eval;
}

int evalBidding(int cards[][MAX_ROUNDS], int player, int illegalBid){
	double score = 0.0;
	double cValue = 0.0;
	for (int i = 0; i < numberOfSuits; i++){
		for(int j = 0; j < numberOfRounds; j++){
			if(cards[i][j] == player){ //card is owned by player
				if(i == currentTrump){ //card is trump
					cValue = (1.0 - (double)(abs(j - orderSize)) / (numberOfCards));
				}
				else { //card isnt trump
					cValue = (1.0 - (double)((pow((double)(j - orderSize), 2.0)+ orderSize) / (numberOfCards)));
				}
				if (cValue > 0){
					score += cValue;
				}
			}
		}
	}
	if (round(score) == illegalBid){
		if(round(score+0.5) == illegalBid){
			score = (round(score)-1.0);
		}
		else {
			score = round(score+0.5);
		}
	}
	else {
		score = round(score);
	}
	return score;
}

int evalWinPoints(int nRounds){ //calculates how many points ONE player can win from the current round onwards
	return (nRounds*(nRounds +1)/2)*WIN_FACT + WIN_CONST*nRounds;
}

int evalLosePoints(int nRounds){ //calculates how many points ONE player can lose from the current round onwards
	return (nRounds*(nRounds +1)/2)*LOSE_FACT;
}

//evaluates the quality of the scoreboard for the player passed as parameter
float evalScoreBoard(int player, int curRound, int scoreboard[]){
	int normalize = scoreboard[player];
	float adjustedScore[numberOfPlayers];
	float SBvalue = 0.0;
	for(int i = 0; i < numberOfPlayers; i++){ //normalize the array on the player
		adjustedScore[i] = scoreboard[i] - normalize;
		if(adjustedScore[i] == 0.0 && i != player){
			adjustedScore[i] = 1.0;
		}
	}

	sort(adjustedScore, adjustedScore+numberOfPlayers, greater<int>()); //sort the array
	adjustedScore[0] = adjustedScore[0] * pow(adjustedScore[0], adjustedScore[0]/(curRound-1+WIN_CONST+curRound));

	for(int i = 0; i < numberOfPlayers; i++){
		SBvalue = SBvalue - (adjustedScore[i]/(i+1));
	}

	return SBvalue;
}

/******************************************************************************
 * CHECKER Functions:
 * These functions are used to get data for any player from the game
 * - Determine who has won the trick -> trickWinner()
 * - Check if a move is legit -> validMove()
 * - Determine the game winner -> gameWinner()
 * - absolute victor -> Check if someone has won the game before it is over
 * - Possible moves -> returns legal moves for a certain player
 * - playMove -> Plays a move and checks if that is a legal move
 */

//Checks who won the trick
int trickWinner(int playedMoves[][CARD_DIM]){ //returns the winning hand
	int winningHand = 0;
	for(int i = 1; i < numberOfPlayers; i++){
		if(playedMoves[i][0] == -1){ //trick isnt complete yet!
			//cout << "oy vey"
			return -i; //cant decide a winner then
		}
		if(playedMoves[i][0] == playedMoves[0][0]){ //hand followed suit
			if(playedMoves[i][0] == playedMoves[winningHand][0]){ //checking if suits match against current winner
				if(playedMoves[i][1] > playedMoves[winningHand][1]){ //check if current is higher
					winningHand = i; //current hand is the best
				}
			}
		}
		else if (currentTrump == playedMoves[i][0]){ //hand is trump suit
			if(playedMoves[i][0] == playedMoves[winningHand][0]){ //current winning hand also played trump suit
				if(playedMoves[i][1] > playedMoves[winningHand][1]){ //check if current is higher
					winningHand = i; //current hand is the best
				}
			}
			else { //current winning hand didn't play trump
				winningHand = i; //current hand is the best
			}
		} //player did not follow suit or play trump therefore not eligible to win trick
	}

	return winningHand; //return the player NUMBER that won (so the actual player, not the hand)
}

//Checks if a move is legal
bool validMove(int cards[][MAX_ROUNDS], int playedMoves[][CARD_DIM], int player, int pSuit, int pCardn){
	if(cards[pSuit][pCardn] == player){ //card belongs to the player
		if (playedMoves[0][0] == -1 || pSuit == playedMoves[0][0]){ //first card to be played or card suit matches
			return true;
		}
		else { //The player does not follow suit
			for(int i = 0; i < numberOfRounds; i++){
				if(cards[playedMoves[0][0]][i] == player){ //the player could follow suit
					return false;
				}
			} //the player could not follow suit
			return true;
		}
	}
	return false; //the card does not belong to the player
}

//Checks who has won, tie means nobody won
int gameWinner(int score[]){
	bool tie = false;
	int winner = 0;
	for (int i = 1; i < numberOfPlayers; i++){
		if(score[i] > score[winner]){
			winner = i;
			tie = false;
		}
		else if (score[i] == score[winner]){
			tie = true;
		}
	}
	if (tie){
		return -1;
	}
	return winner;
}

int absoluteVictor(int score[], int roundsToPlay){ //current scoreboard and how many rounds are yet to be played
	if(roundsToPlay < 2){ //??????? Usefull / useless??
	 	return -1;
	}
	int p1 = 0;
	int p2 = 1;
	if(score[p1] < score[p2]){
		p1 = 1;
		p2 = 0;
	}
	for(int i = 2; i < numberOfPlayers; i++){
		if(score[i] > score[p1]){
			p2 = p1;
			p1 = i;
		}
		else if (score[i] > score[p2]){
			p2 = i;
		}
	}
	if(score[p1] == score[p2]){
		return -1;
	}
	//calculate the distance
	int minP1 = score[p1] - evalLosePoints(roundsToPlay);
	int maxP2 = score[p2] + evalWinPoints(roundsToPlay);


	if(minP1 > maxP2){
		//cout << minP1 << ", " << maxP2 << endl;
		return p1;
	}
	return -1;
}

//Places in the array playable cards which cards are possible to be played
void possibleMoves(int playableCards[][CARD_DIM], int cards[][MAX_ROUNDS], int &numMoves, int playedMoves[][CARD_DIM], int player, int curRound){
	numMoves = 0;
	for(int i = 0; i < numberOfSuits; i++){
		for(int j = 0; j < numberOfRounds; j++){
			if(validMove(cards, playedMoves, player, i, j)){
				playableCards[numMoves][0] = i;
				playableCards[numMoves][1] = j;
				numMoves++;
			}
		}
	}
}

//tries to play a move, returns false if it fails
bool playMove (int player, int suit, int order, int cards[][MAX_ROUNDS], int playedMoves[][CARD_DIM]){
	if(!validMove(cards, playedMoves, player, suit, order)){ //card is not owned, therefore unplayable
		return false;
	}
	for(int i = 0; i < numberOfPlayers; i++){
		if(playedMoves[i][0] == -1){ //was able to play the move
			playedMoves[i][0] = suit;
			playedMoves[i][1] = order;
			cards[suit][order] = -2; // -2 indicates that this card was played
			if(playedMoves[0][0] != playedMoves[i][0]){ //The player did not follow suit
				suitPlayer[playedMoves[0][0]][player] = true; //updating public information
			}
			return true;
		}
	}
	return false; //no place available
}

/******************************************************************************
 * SET-UP and GAME CONTROLLER Functions
 * These functions are used to generate/control rounds/games, so tricks can be played
 * as well as initialize a game and adjust the player order
*/

//shifts an array left
void shiftArray(int array[], int length){
	int temp = array[0];
	for(int i = 1; i < numberOfPlayers; i++){
		array[i-1] = array[i];
	}
	array[length-1] = temp;
}

//Assigns a player number to a card, the indexes determine which card it is, the value stored to whom it belongs (-1 is nan)
void assignCards(int cards[][MAX_ROUNDS], int curRound){
	for (int i = 0; i < numberOfPlayers; i++){ //for each player
		int assigned = 0;
		while(assigned < curRound){ //assign round number of cards
			int suit = rand() % numberOfSuits;
			int number = rand() % (numberOfCards / numberOfSuits);
			if (cards[suit][number] == -1){ //card is free!
				cards[suit][number] = i; //assign it to me!
				assigned++;
			}
		}
	}
	if(curRound < numberOfRounds){ //assign Trump suit if possible
		currentTrump = -1;
		//random starting position
		int suit = rand() % numberOfSuits;
		int number = rand() % (numberOfCards / numberOfSuits);

		for (int i = suit; i < numberOfSuits*2; i++){
			for (int j = 0; j < (numberOfCards / numberOfSuits); j++){
				if (i == suit && j == 0){
					j = number;
				}
				if (cards[i%numberOfSuits][j] == -1){
					currentTrump = i%numberOfSuits;
					cards[i%numberOfSuits][j] = -2; //card has been selected as trump and is made known to the players
					break;
				}
			}
			if (currentTrump > -1){
				break;
			}
		}
	}
}

//Controller function of rounds setup
void initializeRound(int trickScore[], int roundNumber, int cards[][MAX_ROUNDS], int played[][CARD_DIM], int order[], int trickOrder[], bool pSuits[][MAX_PLAYERS]){
	for(int i = 0; i < numberOfSuits; i++){ //initialize card ownership
		for (int j = 0; j < numberOfRounds; j++){
			cards[i][j] = -1;
		}
	}
	for(int i = 0; i < numberOfPlayers; i++){ //initialize arrays
		played[i][0] = -1;
		played[i][1] = -1;
		trickScore[i] = 0;
		for (int j = 0; j < numberOfSuits; j++){
			pSuits[j][i] = false;
		}
	}

	if(roundNumber == numberOfRounds){ //first round
		for(int i = 0; i < numberOfPlayers; i++){ //assign the order
			while(true){ //not very neato but does its job
				int place = rand() % 4;
				if(order[place] == -1){
					order[place] = i;
					trickOrder[place] = i;
					break;
				}
			}
		}
	}
	else { //not first round, shift the order
		int temp = order[0];
		for(int i = 1; i < numberOfPlayers; i++){
			order[i-1] = order[i];
			trickOrder[i-1] = order[i];
		}
		order[numberOfPlayers-1] = temp;
		trickOrder[numberOfPlayers-1] = temp;
	}
	assignCards(cards, roundNumber);
}

void initializeGame(int scores[], int tricks[], int order[]){
	for (int i = 0; i < numberOfPlayers; i++){
		scores[i] = 0;
		tricks[i] = 0;
		order[i] = -1;
	}
}

/******************************************************************************
 * PLAYOUT Functions
 * These are assisting functions for the MC agents, playouts return a value
 * of the quality of the playout for a certain player between [0, 1]
*/
void calculateRequiredCardsSim(int player, int requiredCards[], int tricksMade[],
	 														 int trickCards[][CARD_DIM], int tOrder[],
															 int curRound, int &tNum, int &nPlayer){
	int trickNum = 0;
	for(int i = 0; i < numberOfPlayers; i++){
		trickNum += tricksMade[i];
	}
	int nextPlayer = numberOfPlayers; //assume the trick is already complete
	for(int i = 0; i < numberOfPlayers; i++){
		if(trickCards[i][0] == -1){ //this player hasnt played yet
			nextPlayer = i;
			break;
		}
		requiredCards[tOrder[i]] = 0;
	}
	for(int i = nextPlayer; i < numberOfPlayers; i++){
		requiredCards[tOrder[i]] = 1;
	}
	for(int i = 0; i < numberOfPlayers; i++){
		requiredCards[i] += (curRound-trickNum-1);
	}
	requiredCards[player] = 0;
	tNum = trickNum;
	nPlayer = nextPlayer;
}

bool checkValidCards(int player, int availableCards[], int assignCards[],
	 									 int cards[][MAX_ROUNDS], bool blockedSuits[][MAX_PLAYERS]){
	int reqTest[numberOfPlayers] = {};
	int totalAssign = 0;
	for(int i = 0; i < numberOfPlayers; i++){
		if(i == player){
			continue;
		}
		reqTest[i] = assignCards[i];
		totalAssign += assignCards[i];
		if(availableCards[i] < assignCards[i]){
			return false;
		}
	}

	bool notDone = true;
	float values[numberOfSuits][numberOfRounds] = {};
	while(notDone){
		notDone = false;
		bool assignedOne = false;
		int oneP = -1;
		int oneSuit = -1;
		for(int i = 0; i < numberOfSuits; i++){
			int count = 0;
			int lastP = -1;
			for(int p = 0; p < numberOfPlayers; p++){
				if(p == player){
					continue;
				}
				if(!blockedSuits[i][p] && reqTest[p] > 0){
					lastP = p;
					count++;
				}
			}
			float value = 0;
			if(count > 0){
				value = (float)1 / (float)count;
			}
			for(int j = 0; j < numberOfRounds; j++){
				if(values[i][j] > -0.01 && cards[i][j] == -1){
					values[i][j] = value;
					if(!assignedOne && value == 1.0){
						assignedOne = true;
						oneP = lastP;
						oneSuit = i;
					}
				}
				else{
					values[i][j] = 0;
				}
			}
		}

		if(assignedOne){
			int temppp = 0;
			for(int j = 0; j < numberOfRounds; j++){
				if(values[oneSuit][j] == 1.0){
					temppp++;
					values[oneSuit][j] = -1.0;
					reqTest[oneP]--;
					if(!notDone){
						notDone = true;
					}
				}
			}
		}
	}
	float cardPart[numberOfPlayers] = {};
	for(int i = 0; i < numberOfSuits; i++){
		for(int p = 0; p < numberOfPlayers; p++){
			if(p == player){
				continue;
			}
			if(!blockedSuits[i][p]){
				for(int j = 0; j < numberOfRounds; j++){
					cardPart[p] += values[i][j];
				}
			}
		}
	}

	float donation[numberOfPlayers][numberOfSuits] = {};
	int bums = 0;
	for(int p = 0; p < numberOfPlayers; p++){
		if(p == player){
			continue;
		}
		if(cardPart[p] > (float)reqTest[p] && reqTest[p] > 0){ //have some to spare && actually still part taking in the divide, so the donation is valid
			for(int s = 0; s < numberOfSuits; s++){
				// if(blockedSuits[s][p]){ //cant donate from a blocked suit
				// 	continue;
				// }
				int suitAvailable = 0;
				for(int k = 0; k < numberOfRounds; k++){
					if(cards[s][k] == -1){
						suitAvailable++;
					}
				}
				if(suitAvailable > 0){
					donation[p][s] = (cardPart[p] - (float)reqTest[p]);
					if(donation[p][s] > (float)suitAvailable){ //donating more in this suit than is possible
						donation[p][s] = (float)suitAvailable; //donate the maximum possible;
					}
				}
			}
		}
		else if(cardPart[p] + 0.01f < (float)reqTest[p]){ //need some of that spare
			bums++;
		}
	}
	bool impossible = false;

	if(bums > 0){ //there are agents that might not make it
		for(int p = 0; p < numberOfPlayers; p++){
			if(p == player){
				continue;
			}
			if(cardPart[p] + 0.01f < (float)reqTest[p]){ //this player is a bum
				float usedDonation = 0.0f;
				for(int i = 0; i < numberOfPlayers; i++){ //look for a donator
					bool success = false;
					if(i == player || i == p || (cardPart[i] + 0.01f < (float)reqTest[i])){ //cant loan from the actual player or yourself or another bum
						continue;
					}
					for(int j = 0; j < numberOfSuits; j++){
						// if(blockedSuits[j][p]){ //cant loan from a blocked suit
						// 	continue;
						// }
						//Donator may also be able to donate through another donator..
						if(donation[i][j] > 0.0f){ //the donator has some room in this suit
							float localValue = donation[i][j];
							usedDonation += localValue;
							//donation[i][j] = 0.0f;
							if((cardPart[p] + usedDonation + 0.01f) >= (float)reqTest[p]){ // now have enough
								success = true;
								if(cardPart[p] + usedDonation > (float)reqTest[p] + 0.01f){ //used more than needed
									localValue -= (cardPart[p] + usedDonation - (float)reqTest[p]);
									usedDonation -= (cardPart[p] + usedDonation - (float)reqTest[p]);
								}
							}
							for(int k = 0; k < numberOfSuits; k++){
								donation[i][k] -= localValue;
								if(donation[i][k] < 0.0f){
									donation[i][k] = 0.0f;
								}
							}
						}
						if(success){
							break;
						}
					}
				}
				if((cardPart[p] + usedDonation + 0.01f) < (float)reqTest[p]){ //see if it now fits after borrowing
					impossible = true;
					break;
				}
			}
		}
	}

	if(impossible){
		return false;
	}
	return true;
}

void playSimulatedMove(int simPlayer, int suit, int order, int cards[][MAX_ROUNDS],
					   int playedMoves[][CARD_DIM], bool suits[][MAX_PLAYERS]){
	for(int i = 0; i < numberOfPlayers; i++){
		if(playedMoves[i][0] == -1){ //was able to play the move
			playedMoves[i][0] = suit;
			playedMoves[i][1] = order;
			cards[suit][order] = -2; // -2 indicates that this card was played
			if(playedMoves[0][0] != playedMoves[i][0]){ //did not follow suit
				suits[playedMoves[0][0]][simPlayer] = true;
			}
			return;
		}
	}
}

//Made for agents that work within the incomplete information
void simulatePossibleMoves(int player, int simP, int currentTrick[][CARD_DIM],
	 												 int assignCards[], int posmovsim[][CARD_DIM],
													 int cards[][MAX_ROUNDS], bool blockedSuits[][MAX_PLAYERS],
													 int &numMoves){
	int availableCards[numberOfPlayers] = {};
	int trickPos = 0;
	for(int i = 0; i < numberOfSuits; i++){
		if(i < numberOfPlayers && currentTrick[i][0] != -1){
			trickPos = i;
		}
		for(int j = 0; j < numberOfRounds; j++){
			if(cards[i][j] != -1){
				continue;
			}
			for(int k = 0; k < numberOfPlayers; k++){
				if(k == player){
					continue;
				}
				if(!blockedSuits[i][k]){
					availableCards[k]++;
				}
			}
		}
	}
	int curpos = 0;
	for(int i = 0; i < numberOfSuits; i++){
		bool suitVerified = false;
		if(blockedSuits[i][simP]){
			continue;
		}
		for(int j = 0; j < numberOfRounds; j++){
			if(cards[i][j] == -1){ //possible candidate
				if(!suitVerified){
					int saveSuit = i; //the suit bool value before the simulated move
					if(currentTrick[0][0] != -1){
							saveSuit = currentTrick[0][0];
					}

					//pretend you played this card
					playSimulatedMove(simP, i, j, cards, currentTrick, blockedSuits);
					assignCards[simP]--;
					//check if the cards then still works out
					bool check = checkValidCards(player, availableCards, assignCards, cards, blockedSuits);
					//put everything back
					assignCards[simP]++;
					cards[i][j] = -1;
					currentTrick[trickPos+1][0] = -1;
					currentTrick[trickPos+1][1] = -1;
					blockedSuits[saveSuit][simP] = false; //(possibly) set it back to false
					if(check){
						suitVerified = true;
					}
					else{ //we cant take any cards of this suit
						break;
					}
				}
				//if you made it here the card is good!
				posmovsim[curpos][0] = i;
				posmovsim[curpos][1] = j;
				curpos++; //another possible move detected
			}
		}
	}
	numMoves = curpos;
}


bool generateSimulatedHands(int player, int assignCards[], int cards[][MAX_ROUNDS],
	 												  bool blockedSuits[][MAX_PLAYERS]){
	int availableCards[numberOfPlayers] = {};
	int cardsToPlace = 0;
	for(int i = 0; i < numberOfPlayers; i++){
		if(i == player){
			continue;
		}
		cardsToPlace += assignCards[i];
	}
	for(int j = 0; j < numberOfSuits; j++){
		for(int k = 0; k < numberOfRounds; k++){
			if(cards[j][k] == -1){
				for(int i = 0; i < numberOfPlayers; i++){
					if(i == player){
						continue;
					}
					if(!blockedSuits[j][i]){
						availableCards[i]++;
					}
				}
			}
		}
	}

	// if(!checkValidCards(player, availableCards, assignCards, cards, blockedSuits)){
	// 	cout << "check valid false in generateSimulatedHands()" << endl;
	// 	return false;
	// }

	for(int a = 0; a < cardsToPlace; a++){ //
		int suit = rand() % numberOfSuits;
		int number = rand() % numberOfRounds;
		int pSelected = rand() % numberOfPlayers;
		while(pSelected == player){
			pSelected = rand() % numberOfPlayers;
		}

		float pRatio;
		if(availableCards[pSelected] > 0){
		  pRatio = (float)assignCards[pSelected] / (float)availableCards[pSelected];
		}
		else {
		  pRatio = 0;
		}

		for(int i = 0; i < numberOfPlayers; i++){ //selected the best candidate
		  if(i == player){
		    continue;
		  }
		  float curRatio = (float)assignCards[i] / (float)availableCards[i];
		  if (pRatio < curRatio || (pRatio == curRatio && availableCards[i] < availableCards[pSelected])){ //select the player that needs it the most
		    pSelected = i;
		    pRatio = curRatio;
		  }
		}

		//select a card for the player
		bool foundCard = false;
		int test = -1;
		while(!foundCard){
			test++;
			if(test > numberOfCards){ //ran through all the cards and still couldnt find a usefull one
				return false;
			}

			//check if the suit is right
			if((number > 0 && number % numberOfRounds == 0) || blockedSuits[suit][pSelected]){
				//cout << number % numberOfRounds << endl;
				number = 0;
				suit = (suit+1) % numberOfSuits;
			}
			if(cards[suit][number] != -1){ //card is not available
				number++;
				continue;
			}

			//calculate if taking this card ruins it for the other players combined
			int combinedCards = 0;
			for(int i = 0; i < numberOfSuits; i++){
				for(int j = 0; j < numberOfRounds; j++){
					if(cards[i][j] != -1 || (i == suit && j == number)){
						continue;
					}
					for(int k = 0; k < numberOfPlayers; k++){
						if(k == pSelected || k == player){
							continue;
						}
						if(!blockedSuits[i][k]){
							combinedCards++;
							break;
						}
					}
				}
			}
			int combinedReq = 0;
			for(int i = 0; i < numberOfPlayers; i++){
				if(i == pSelected || i == player){
					continue;
				}
				if(!blockedSuits[suit][i] && assignCards[i] == availableCards[i]){ //cant take this suit due to other players
					number = 0;
					suit = (suit+1) % numberOfSuits;
					continue;
				}
				combinedReq += assignCards[i];
			}

			if(combinedCards < combinedReq){ //cant take this suit due to other players
				number = 0;
				suit = (suit+1) % numberOfSuits;
				continue;
			}

			if(!blockedSuits[suit][pSelected] && cards[suit][number] == -1){
				break; //found a matching card
			}
			number++;
		}
		cards[suit][number] = pSelected;
		assignCards[pSelected]--;

		for(int j = 0; j < numberOfPlayers; j++){
			if(j == player){
				continue;
			}
			if(!blockedSuits[suit][j]){
				availableCards[j]--;
			}
			if (assignCards[j] > availableCards[j]){
				return false;
			}
		}
	}
	return true;
}

//plays randomly till the end, results are stored in the scoreboard
bool playoutRoundRandom(int player, int curRound, int cards[][MAX_ROUNDS], int tOrder[],
						 int score[], int trickCards[][CARD_DIM], int tricksMade[],
						 int bids[], bool suits[][MAX_PLAYERS]){

	int possibleCards[curRound][CARD_DIM];
	int requiredCards[numberOfPlayers] = {};
	int trickNum = 0;
	int nextPlayer = -1;

	calculateRequiredCardsSim(player, requiredCards, tricksMade, trickCards, tOrder, curRound, trickNum, nextPlayer);
	if(!generateSimulatedHands(player, requiredCards, cards, suits)){
		return false;
	}

	for(int numTricks = trickNum; numTricks < curRound; numTricks++){ //play all tricks left in the round
		for(int a = nextPlayer; a < numberOfPlayers; a++){ //play a trick
			int moves = 0;
			int move = -1;
			int suitChoice = -1;
			int orderChoice = -1;

			for(int i = 0; i < curRound; i++){ //clear the possible moves
				possibleCards[i][0] = -1;
				possibleCards[i][1] = -1;
			}
			possibleMoves(possibleCards, cards, moves, trickCards, tOrder[a], curRound);
			//because the hands are generated we use possibleMoves() instead of simulatePossibleMoves()
			if(moves == 0){
				cout << "!!!!!!!!!!!!!!!!!!!" << endl;
			}
			move = rand() % moves;
			suitChoice = possibleCards[move][0];
			orderChoice = possibleCards[move][1];
			playSimulatedMove(tOrder[a], suitChoice, orderChoice, cards, trickCards, suits);
		}

		nextPlayer = 0; //trick done, start over from the top
		int winner = tOrder[trickWinner(trickCards)];
		//cout << "que " << winner << endl;
		tricksMade[winner]++;

		while(tOrder[0] != winner){ //trick winner starts next trick
			shiftArray(tOrder, numberOfPlayers);
		}
		for(int i = 0; i < numberOfPlayers; i++){ //reset the cards on the table
			trickCards[i][0] = -1;
			trickCards[i][1] = -1;
		}
	}

	for (int i = 0; i < numberOfPlayers; i++){
		if(bids[i] == tricksMade[i]){ //won as much as you bid, so score += x + y*c
			score[i] += (WIN_CONST + tricksMade[i]*WIN_FACT); //room for factoring
		}
		else { //over or under shot, so score -= abs(x - y)*c
			score[i] -= (abs(bids[i] - tricksMade[i])*LOSE_FACT); //room for factoring
		}
	}
	return true;
}

/*****************************************************************************
* Monte Carlo Bidding Function
*/

int evalBidMC(int cards[][MAX_ROUNDS], int player, int illegalBid, int trickOrder[], int currentRound){
	int totalTricksMade = 0;
	int fakeBids[numberOfPlayers] = {};
	int fakeTricksMade[numberOfPlayers] = {};
	int fakeScore[numberOfPlayers] = {};
	bool fakeSuit[numberOfPlayers][MAX_PLAYERS] = { false }; //at the beginning of a game, nobody has failed to match suits
	int fakeTrick[numberOfPlayers][CARD_DIM] = {};
	int copyCards[numberOfSuits][MAX_ROUNDS];
	int copyTrickOrder[numberOfPlayers];
	copy(&cards[0][0], &cards[0][0]+numberOfSuits*MAX_ROUNDS, &copyCards[0][0]);
	for(int i = 0; i < numberOfPlayers; i++){
		fakeTrick[i][0] = -1;
		fakeTrick[i][1] = -1;
		copyTrickOrder[i] = trickOrder[i];
	}
	int spend = 0;
	while(spend < mcBudget){
		if(!playoutRoundRandom(player, currentRound, copyCards, copyTrickOrder, fakeScore, fakeTrick, fakeTricksMade, fakeBids, fakeSuit)){
			cout << "Playout went wrong at bidding???" << endl;
			string t;
			cin >> t;
		}
		totalTricksMade += fakeTricksMade[player];
		//reset everything
		copy(&cards[0][0], &cards[0][0]+numberOfSuits*MAX_ROUNDS, &copyCards[0][0]);
		for(int i = 0; i < numberOfPlayers; i++){
			fakeTrick[i][0] = -1;
			fakeTrick[i][1] = -1;
			fakeTricksMade[i] = 0;
			fakeScore[i] = 0;
			copyTrickOrder[i] = trickOrder[i];
			for(int j = 0; j < numberOfSuits; j++){
				fakeSuit[j][i] = false;
			}
		}
		spend++;
	}
	double score = (double)totalTricksMade / (double)mcBudget; //average tricksMade
	if (round(score) == illegalBid){
		if(round(score+0.5) == illegalBid){
			score = (round(score)-1.0);
		}
		else {
			score = round(score+0.5);
		}
	}
	else {
		score = round(score);
	}
	return (int)score;
	return 0;
}

/******************************************************************************
 * Monte Carlo Tree FUNCTIONS:
 * These functions manage the tree for MCTS
 */

double calculateMCTSNodeValue(mctsNode * node){
	return (node->evaluations / (double)node->simulations) + (MCTS_CONSTANT * sqrt((log(node->parent->simulations)/node->simulations)));
}

bool expandMCTSNode(mctsNode * parent, int currentRound, int player, int score[], int bids[]){
	//checking what the situation is of the current trick
	int currentTrickPos = 0;
	int dummyPlayed[numberOfPlayers][CARD_DIM];
	int dummyCards[numberOfSuits][MAX_ROUNDS];
	bool dummySuits[numberOfSuits][MAX_PLAYERS];
	int dummyTrickOrder[numberOfPlayers];
	//duplicating some info of the parent in dummy variables, to simulate a move
	copy(&parent->curTrick[0][0], &parent->curTrick[0][0]+numberOfPlayers*CARD_DIM, &dummyPlayed[0][0]);
	copy(&parent->cardInfo[0][0], &parent->cardInfo[0][0]+numberOfSuits*MAX_ROUNDS, &dummyCards[0][0]);
	copy(&parent->blockedSuits[0][0], &parent->blockedSuits[0][0]+numberOfSuits*MAX_PLAYERS, &dummySuits[0][0]);
	copy(&parent->trickOrder[0], &parent->trickOrder[0]+numberOfPlayers, &dummyTrickOrder[0]);

	for(int i = 0; i < numberOfPlayers; i++){
		if(dummyPlayed[i][0] == -1){
			break;
		}
		currentTrickPos++;
	}

	int currentPlayer;
	if(currentTrickPos == numberOfPlayers){//parent completed a trick, start a new one
		currentPlayer = parent->trickOrder[trickWinner(dummyPlayed)];
		while(dummyTrickOrder[0] != currentPlayer){ //set the winner
			shiftArray(dummyTrickOrder, numberOfPlayers);
		}
		for(int i = 0; i < numberOfPlayers; i++){
			//erase the previous trick
			dummyPlayed[i][0] = -1;
			dummyPlayed[i][1] = -1;
			currentTrickPos = 0;
		}
	}
	else { //trick not done yet
		currentPlayer = parent->trickOrder[currentTrickPos];
	}
	int numberOfUnexplored;
	//play a card into the dummyPlayed, defined by the parent
	for(int i = 0; i < MAX_CARDS - MAX_ROUNDS; i++){
		if(parent->unexploredMoves[i][0] == -1){
			numberOfUnexplored = i;
			break;
		}
	}

	int choice = rand() % numberOfUnexplored;
	playSimulatedMove(currentPlayer, parent->unexploredMoves[choice][0], parent->unexploredMoves[choice][1], dummyCards, dummyPlayed, dummySuits);

	//update the parent that a selection has been made
	//selected element becomes last element
	parent->unexploredMoves[choice][0] = parent->unexploredMoves[numberOfUnexplored-1][0];
	parent->unexploredMoves[choice][1] = parent->unexploredMoves[numberOfUnexplored-1][1];

	//wipe out last element as it is now a duplicate
	parent->unexploredMoves[numberOfUnexplored-1][0] = -1;
	parent->unexploredMoves[numberOfUnexplored-1][1] = -1;

	//now decide what your possible children are going to be
	int nextPlayer = trickWinner(dummyPlayed);
	nextPlayer = dummyTrickOrder[abs(nextPlayer)]; //works for all tricks!
	int dummyNextPlayerMoves[numberOfCards][CARD_DIM] = {}; //could be optimized
	int numberOfMoves;

	int dummyNextPlayed[numberOfPlayers][CARD_DIM];
	int trickScore[numberOfPlayers];
	copy(&parent->tricksMade[0], &parent->tricksMade[0]+numberOfPlayers, &trickScore[0]);
	int checkWinner = trickWinner(dummyPlayed);

	if(checkWinner >= 0){ //some hand won in the parent, adjust the score
		trickScore[parent->trickOrder[checkWinner]]++;
	}

	if(dummyPlayed[numberOfPlayers-1][0] == -1){ //the trick is NOT full
		copy(&dummyPlayed[0][0], &dummyPlayed[0][0]+numberOfPlayers*CARD_DIM, &dummyNextPlayed[0][0]);
	}
	else { //THIS node completed the current TRICK
		for(int i = 0; i < numberOfPlayers; i++){
			dummyNextPlayed[i][0] = -1;
			dummyNextPlayed[i][1] = -1;
		}
	}
	if(nextPlayer == player){ //the next player is the actual player running the tree
		possibleMoves(dummyNextPlayerMoves, dummyCards, numberOfMoves, dummyNextPlayed, player, currentRound);
	}
	else {
		//calculate how much cards each player requires, except for the actual player
		int reqCards[numberOfPlayers] = {};
		int temp;
		calculateRequiredCardsSim(player, reqCards, trickScore, dummyNextPlayed, dummyTrickOrder, currentRound, temp, temp);
		simulatePossibleMoves(player, nextPlayer, dummyNextPlayed, reqCards, dummyNextPlayerMoves, dummyCards, dummySuits, numberOfMoves);
	}


	//now we can actually create the node now we have all the information needed
	mctsNode * child = new mctsNode(parent, currentPlayer);
	copy(&dummyCards[0][0], &dummyCards[0][0]+numberOfSuits*MAX_ROUNDS, &child->cardInfo[0][0]);
	copy(&dummyPlayed[0][0] , &dummyPlayed[0][0]+numberOfPlayers*CARD_DIM, &child->curTrick[0][0]);
	copy(&dummyTrickOrder[0], &dummyTrickOrder[0]+numberOfPlayers, &child->trickOrder[0]);
	copy(&dummySuits[0][0], &dummySuits[0][0]+numberOfSuits*MAX_PLAYERS, &child->blockedSuits[0][0]);
	copy(&trickScore[0], &trickScore[0]+numberOfPlayers, &child->tricksMade[0]);

	if(numberOfMoves == 0){
		int totalTricks = 0;
		for(int i = 0; i < numberOfPlayers; i++){
			totalTricks += trickScore[i];
		}
		if(totalTricks < currentRound){
			return false;
		}
		else { //zero moves but it is a leaf!
			return true;
		}
	}

	//duplicate the possible moves from this node beyond
	for(int i = 0; i < numberOfMoves; i++){
		child->unexploredMoves[i][0] = dummyNextPlayerMoves[i][0];
		child->unexploredMoves[i][1] = dummyNextPlayerMoves[i][1];
	}

	return playoutRoundRandom(player, currentRound, dummyCards, dummyTrickOrder, score, dummyPlayed, trickScore, bids, dummySuits);
}

void deleteMCTSTree(mctsNode * node){
	int child = 0;
	while(node->children[child] != NULL && child < (MAX_CARDS - MAX_ROUNDS)){
		deleteMCTSTree(node->children[child]);
		child++;
	}
	delete node;
}

/******************************************************************************
 * Agent FUNCTIONS:
 * These functions allow for a player to play with a certain agent.
 */

int randomAgent(int playableCards[][CARD_DIM]){
	int numberOfMoves = 0;
	for (int i = 0; i < numberOfRounds; i++){
		if(playableCards[i][0] == -1){
			break;
		}
		numberOfMoves += 1;
	}
	return rand() % numberOfMoves;
}

int trumpingAgent(int playableCards[][CARD_DIM]){
	int numberOfMoves = 0;
	int move = -1;
	stack<int> trumpStack;
	for (int i = 0; i < numberOfRounds; i++){
		if(playableCards[i][0] == -1){
			break;
		}
		if (playableCards[i][0] == currentTrump){
			trumpStack.push(i);
		}
		numberOfMoves += 1;
	}
	if (trumpStack.size() > 0){
		move = rand() % trumpStack.size();
		for(int i = 0; i < move; i++){
			trumpStack.pop();
		}
		move = trumpStack.top();
	}
	else {
		move = rand() % numberOfMoves;
	}
	return move;
}

int ruleBasedAgent(int player, int tOrder[], int playedCards[][CARD_DIM],
	 								 int playableCards[][CARD_DIM], int cards[][MAX_ROUNDS],
									 int bid, int nTricks){
	int move = 0;
	int elPlayers = numberOfPlayers-1; //The number of elligible winners that haven't played yet
	double evaluation[numberOfRounds];
	stack<int> winningStack; //stack of cards that still could possibly win the trick
	stack<int> losingStack; //stack of cards evaluated at zero
	for (int i = 0; i < numberOfPlayers; i++){ //check players that have not yet played
		if( playedCards[i][0] == -1 &&
			suitPlayer[playedCards[0][0]][tOrder[i]] == true &&
			suitPlayer[currentTrump][tOrder[i]]){ //player cannot win this trick
			elPlayers--;
		}
	}
	for (int i = 0; i < numberOfRounds; i++){
        if(playableCards[i][0] == -1){
            break;
        }
		evaluation[i] = evalCardCurrent(cards, playedCards, player, playableCards[i][0], playableCards[i][1]);  //evaluate the card here
		if(evaluation[i] <= 0.0){
			losingStack.push(i);
		}
		else {
			winningStack.push(i);
		}
	}

	if(nTricks < bid){ //try to win this trick
		if(winningStack.size() == 0){ // no good cards
			move = losingStack.top();
			for (unsigned int i = 1; i < losingStack.size(); i++){ //play the worst losing card you have (closest to zero)
				if(evaluation[losingStack.top()] > evaluation[move]){
					move = losingStack.top();
				}
			}
		}
		else if(elPlayers){ //Check if you are the last player or if no other player after you can win
			move = winningStack.top();
			winningStack.pop();
			for (unsigned int i = 1; i < winningStack.size(); i++){
				if (evaluation[winningStack.top()] < evaluation[move]){ //get the worst best move
					move = winningStack.top();
				}
				winningStack.pop();
			}
		}
		else { //play the best card you have
			for (unsigned int i = 1; i < winningStack.size(); i++){
				if (evaluation[winningStack.top()] > evaluation[move]){
					move = winningStack.top();
				}
				winningStack.pop();
			}
		}
	} else { // don't try to win
		if(losingStack.size() > 0){
			move = losingStack.top();
			losingStack.pop();
			while(losingStack.size() > 0){
				if(evaluation[losingStack.top()] < evaluation[move]){
					move = losingStack.top();
				}
				losingStack.pop();
			}
		}
		else { //no definite losing cards
			while(winningStack.size() > 0){
				if(evaluation[winningStack.top()] < evaluation[move]){ //play the best worst move
					move = winningStack.top();
				}
				winningStack.pop();
			}
		}
	}
	return move;
}

int monteCarloAgent(int budget, int player, int curRound, int scoreBoard[],
	 									int tOrder[], int playedCards[][CARD_DIM],
										int playableCards[][CARD_DIM], int cards[][MAX_ROUNDS],
										int bids[], int tricks[]){
	int copyGameCards[numberOfSuits][MAX_ROUNDS];
	int copyPlayed[numberOfPlayers][CARD_DIM];
	int copyTrickOrder[numberOfPlayers];
	int copyScoreBoard[numberOfPlayers];
	int copyTrickScore[numberOfPlayers];
	bool copySuitBools[numberOfSuits][MAX_PLAYERS];
	int currentTrick = 0;
	copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]);
	for (int i = 0; i < numberOfPlayers; i++){
		currentTrick += tricks[i];
		if(i != player){
			copyScoreBoard[i] += WIN_CONST + curRound;
		}
		else {
			copyScoreBoard[i] -= curRound;
		}
	}
	float worstScoreBoardEval = evalScoreBoard(player, curRound, copyScoreBoard);
	copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]);
	for (int i = 0; i < numberOfPlayers; i++){
		if(i != player){
			copyScoreBoard[i] -= curRound;
		}
		else {
			copyScoreBoard[i] += WIN_CONST + curRound;
		}
	}
	float bestScoreBoardEval = evalScoreBoard(player, curRound, copyScoreBoard);
	float simResults[curRound] = {0.0};
	int numberOfMoves = 0;
	for(int i = 0; i < curRound; i++){
		if(playableCards[i][0] == -1){
			break;
		}
		numberOfMoves++;
	}
	for(int i = 0; i < numberOfMoves; i++){
		// if(playableCards[i][0] == -1){ //out of cards
		// 	break;
		// }
		for(int j = 0; j < budget/numberOfMoves; j++){
			//set up the copied game state
			copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]);
			copy(&cards[0][0], &cards[0][0]+numberOfSuits*MAX_ROUNDS, &copyGameCards[0][0]);
			copy(&playedCards[0][0], &playedCards[0][0]+numberOfPlayers*CARD_DIM, &copyPlayed[0][0]);
			copy(&tOrder[0], &tOrder[0]+numberOfPlayers, &copyTrickOrder[0]);
			copy(&tricks[0], &tricks[0]+numberOfPlayers, &copyTrickScore[0]);
			copy(&suitPlayer[0][0], &suitPlayer[0][0]+numberOfSuits*MAX_PLAYERS, &copySuitBools[0][0]);
			//play the selected move
			playSimulatedMove(player, playableCards[i][0], playableCards[i][1], copyGameCards, copyPlayed,copySuitBools);
			playoutRoundRandom(player, curRound, copyGameCards, copyTrickOrder, copyScoreBoard, copyPlayed, copyTrickScore, bids, copySuitBools);
			if(absoluteVictor(copyScoreBoard, curRound-1) == player){
				simResults[i] += 1.0;
			}
			else {
				double eval = ((evalScoreBoard(player, curRound, copyScoreBoard)-worstScoreBoardEval)/ (bestScoreBoardEval-worstScoreBoardEval));
				if(eval > 0){
					simResults[i] += eval;
				}
			}
		}
	}
	int move = 0;
	for (int i = 1; i < curRound; i++){
		if(playableCards[i][0] == -1){
			break;
		}
		if(simResults[move] < simResults[i]){
			move = i;
		}
	}
	return move;
}

int monteCarloTreeSearchAgent(int budget, int player, int curRound,
															int scoreBoard[], int tOrder[],
															int playedCards[][CARD_DIM],
															int playableCards[][CARD_DIM],
															int cards[][MAX_ROUNDS], int bids[], int tricks[]){
	//set up the root node
	int numberOfMoves = 0;
	for(int i = 0; i < curRound; i++){
		if(playableCards[i][0] == -1){
			break;
		}
		numberOfMoves++;
	}

	mctsNode root(NULL, -1);
	mctsNode * rootPointer = &root;

	copy(&cards[0][0], &cards[0][0]+numberOfSuits*MAX_ROUNDS, &root.cardInfo[0][0]);
	copy(&playedCards[0][0], &playedCards[0][0]+numberOfPlayers*CARD_DIM, &root.curTrick[0][0]);
	copy(&tOrder[0], &tOrder[0]+numberOfPlayers, &root.trickOrder[0]);
	copy(&tricks[0], &tricks[0]+numberOfPlayers, &root.tricksMade[0]);
	copy(&suitPlayer[0][0], &suitPlayer[0][0]+numberOfSuits*MAX_PLAYERS, &root.blockedSuits[0][0]);
	copy(&playableCards[0][0], &playableCards[0][0]+numberOfRounds*CARD_DIM, &root.unexploredMoves[0][0]);

	int copyScoreBoard[numberOfPlayers];
	copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]);

	int currentTrick = 0;
	float WSEval[numberOfPlayers] = {}; //worst evaluation
	float BSEval[numberOfPlayers] = {}; //best evaluation

	for(int j = 0; j < numberOfPlayers; j++){ //for each player
		currentTrick += tricks[j];
		for (int i = 0; i < numberOfPlayers; i++){ //adjust the scoreboard to the worst
			if(i != j){
				copyScoreBoard[i] += (WIN_CONST + curRound);
			}
			else {
				copyScoreBoard[i] -= curRound;
			}
		}
		WSEval[j] = evalScoreBoard(j, curRound, copyScoreBoard); //evaluate it
		copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]); //reset
		for (int i = 0; i < numberOfPlayers; i++){ //adjust the scoreboard to the best
			if(i != j){
				copyScoreBoard[i] -= curRound;
			}
			else {
				copyScoreBoard[i] += (WIN_CONST + curRound);
			}
		}
		BSEval[j] = evalScoreBoard(j, curRound, copyScoreBoard); //evaluate it
		copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]); //reset
	}
	copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]);

	mctsNode * currentNode = rootPointer;
	int spend = 0;

	mctsNode * selectedNode;
	double currentEval;

	bool foundNode = false;
	copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]);
	// cout << endl << endl << "--Starting with a new MCTS loop--" << endl;
	while(spend < mcBudget){//loop through the budget
		//cout << "Spend(" << curRound << "): " << spend << "/" << mcBudget << endl;
		copy(&scoreBoard[0], &scoreBoard[0]+numberOfPlayers, &copyScoreBoard[0]);
		foundNode = false;
		currentNode = rootPointer;

		currentEval = 0.0;
		//cout << "going to look for a node" << endl;
		int childNumber = -1;
		while(!foundNode){ //Decide which node to select by looping through the nodes
			if(currentNode == NULL){
				cout << "HELLO" << endl;
			}
			int trickNum = 0;
			for(int i = 0; i < numberOfPlayers; i++){
				trickNum += currentNode->tricksMade[i];
			}
			if(trickNum >= curRound){ //leaf node selected
				break;
			}
			if(currentNode->unexploredMoves[0][0] != -1){ //Node has unexplored children, node found
				break;
			}
			//cout << "Searching through a nodes children..(" << currentNode->numberOfChildren << ")" << endl;
			if(currentNode->numberOfChildren == 0){ //node has no possible children nor any children right now
				if(currentNode->parent->parent == NULL){ //about to the delete a node that is a possible move
					spend = mcBudget;
					leafQuit++;
					break;
				}
				mctsNode * temp = currentNode->parent;
				if(currentNode->parent == NULL){
					cout << leafQuit << endl;
					cout << "WORKING WITH THE ROOT! " << currentNode->numberOfChildren << endl;
					cout << "Spend(" << tempGames << "): " << spend << endl;
					string r;
					cin >> r;
				}
				int trickNum = 0;
				for(int i = 0; i < numberOfPlayers; i++){
					trickNum += currentNode->tricksMade[i];
				}
				if(trickNum >= curRound){ //last trick has been made, thus a leaf node
					break;
				}
				temp->children[childNumber] = temp->children[temp->numberOfChildren-1];
				temp->children[temp->numberOfChildren-1] = NULL;
				temp->numberOfChildren--;

				delete currentNode;
				currentNode = currentNode->parent;
				continue;
			}
			currentEval = 0.0;
			for(int i = 0; i < currentNode->numberOfChildren; i++){ //loop through current nodes children
				double temp = calculateMCTSNodeValue(currentNode->children[i]);
				if(temp > currentEval){
					currentEval = temp;
					selectedNode = currentNode->children[i];
					childNumber = i;
				}
			}
			currentNode = selectedNode;
		}

		int tr = 0;
		for(int i = 0; i < numberOfPlayers; i++){
			tr += currentNode->tricksMade[i];
		}
		if(tr < curRound){
			//Expand the selected node:
			bool succesfullNode = expandMCTSNode(currentNode, curRound, player, copyScoreBoard, bids);
			//check if the node is valid or a leaf node
			int trickNum = 0;
			for(int i = 0; i < numberOfPlayers; i++){
				trickNum += currentNode->children[currentNode->numberOfChildren-1]->tricksMade[i];
			}
			if(trickNum < curRound && (!succesfullNode || currentNode->children[currentNode->numberOfChildren-1]->unexploredMoves[0][0] == -1)){ //the newly created node cant have children
				if(currentNode == NULL){
					cout << "Yowza" << endl;
				}
				if(currentNode->children[currentNode->numberOfChildren-1] == NULL){
					cout << "OH NO" << endl;
				}
				delete currentNode->children[currentNode->numberOfChildren-1];
				currentNode->children[currentNode->numberOfChildren-1] = NULL; //erase the child, it is invalid
				currentNode->numberOfChildren--;
				continue;
			}
			currentNode = currentNode->children[currentNode->numberOfChildren-1];
		}

		//Backpropagation: After the simulation, run the scoreboard up the tree so each node can evaluate it personally
		while(currentNode->parent != NULL){ //while not in the root node
			int playedP = currentNode->player;
			double eval = ((evalScoreBoard(playedP, curRound, copyScoreBoard)-WSEval[playedP])/ (BSEval[playedP]-WSEval[playedP]));
			if(eval > 0){
				currentNode->evaluations += eval;
			}
			currentNode->simulations++;
			currentNode = currentNode->parent;
		}
		currentNode->simulations++;
		spend++;
		simulationsCounter++;
	}
	//select the move with the most simulations
	mctsNode * selectedChild;
	int sim = 0;

	for(int i = 0; i < rootPointer->numberOfChildren; i++){
		if(rootPointer->children[i]->simulations > sim){ //better one found
			selectedChild = rootPointer->children[i];
			sim = rootPointer->children[i]->simulations;
		}
	}
	int childSuit;
	int childOrder;

	for(int i = 0; i < numberOfPlayers; i++){
		if(i+1 == numberOfPlayers || selectedChild->curTrick[i+1][0] == -1){ //found the cards
			childSuit = selectedChild->curTrick[i][0];
			childOrder = selectedChild->curTrick[i][1];
			break;
		}
	}

	int move = 0;
	for(int i = 0; i < rootPointer->numberOfChildren; i++){
		if(playableCards[i][0] == childSuit && playableCards[i][1] == childOrder){
			move = i;
			break;
		}
	}
	//clean up the tree here
	deleteMCTSTree(rootPointer);
	return move; //return your choice
}


/******************************************************************************
 * AGENT CONTROLLER Functions
 * These functions are used decide which agent must be activated at which point in the game
 * as well as supply them with the current information of the game
*/

int playerControllerBid(int algorithm, int player, int cRound, int illegalBid, int gameCards[][MAX_ROUNDS], int tOrder[]){
	int copyCards[numberOfSuits][MAX_ROUNDS];
	for(int i = 0; i < numberOfSuits; i++){
		for(int j = 0; j < numberOfRounds; j++){
			if(gameCards[i][j] == player){
				copyCards[i][j] = player;
			}
			else if(gameCards[i][j] == -2){
				copyCards[i][j] = -2;
			}
			else {
				copyCards[i][j] = -1;
			}
		}
	}
	switch(algorithm){
		case 0: //Random
				return evaluateRandomBid(cRound, illegalBid);
				break;
		case 1: //Bidding Engine
				return evalBidding(copyCards, player, illegalBid);
				break;
		case 2: //MC Bidding
				return evalBidMC(copyCards, player, illegalBid, tOrder, cRound);
				break;
		default:
				return evaluateRandomBid(cRound, illegalBid);
				break;
	}
}

int playerControllerPlay(int algorithm, int player, int cRound, int score[], int tOrder[], int playedCards[][CARD_DIM],
						 int playableCards[][CARD_DIM], int gameCards[][MAX_ROUNDS], int bids[], int tricks[]){
	int privateCards[numberOfSuits][MAX_ROUNDS];
	copy(&gameCards[0][0], &gameCards[0][0]+numberOfSuits*MAX_ROUNDS, &privateCards[0][0]);
	for(int i = 0; i < numberOfSuits; i++){
		for(int j = 0; j < numberOfRounds; j++){
			if(privateCards[i][j] != player && privateCards[i][j] != -2){
				privateCards[i][j] = -1; //make the cards unknown that havent been played and arent yours
			}
		}
	}

	switch(algorithm){
		case 0:	//random
				return randomAgent(playableCards);
				break;
		case 1:
				return trumpingAgent(playableCards);
				break;
		case 2:	//Rule Based Player
				return ruleBasedAgent(player, tOrder, playedCards, playableCards, privateCards, bids[player], tricks[player]);
				break;
		case 3: // Monte Carlo Method
				return monteCarloAgent(mcBudget, player, cRound, score, tOrder, playedCards, playableCards, privateCards, bids, tricks);
				break;
		case 4: // Monte Carlo Tree Search
				return monteCarloTreeSearchAgent(mcBudget, player, cRound, score, tOrder, playedCards, playableCards, privateCards, bids, tricks);
				break;
		default:
				break;
	}
	return 0;
}

void runSimulation(){

}

/******************************************************************************
* HUMAN PLAY FUNCTIONS
* Functions to play against humans
*/

void getCardInput(int &suitN, int &orderN){
	string suit;
	string order;
	int suitNum = -1;
	int orderNum = -1;
	string input;

	while(suitNum == -1){
		cout << "Enter the SUIT NAME of the card: ";
		cin >> suit;
		// cout << endl;
		for(int i = 0; i < numberOfSuits; i++){
			if(suitNames[i].compare(suit) == 0){
				suitNum = i;
				break;
			}
		}
		if(suitNum == -1){
			cout << "! Could not find a suit named: " << suit << ". Lets try again" << endl << endl;
		}
		while(orderNum == -1){
			cout << "Enter the ORDER NAME of the card: ";
			cin >> order;
			// cout << endl;
			for(int i = 0; i < numberOfRounds; i++){
				if(orderNames[i].compare(order) == 0){
					orderNum = i;
					break;
				}
			}
			if(orderNum == -1){
				cout << "! Could not find an order named: " << order << ". Lets try again" << endl << endl;
				continue;
			}
			cout << "If that card is correct type [Y]: ";
			string temp;
			cin >> temp;
			if(temp.compare("Y") == 0){
				break;
			}
			else {
				cout << "Enter the card again." << endl;
				cout << endl;
				suitNum = -1;
				orderNum = -1;
				break;
			}
		}
	}

	suitN = suitNum;
	orderN = orderNum;
}

void setCardNames(){
	suitNames[0] = "Harten";
	suitNames[1] = "Klaver";
	suitNames[2] = "Ruiten";
	suitNames[3] = "Schoppen";
	orderNames[0] = "2";
	orderNames[1] = "3";
	orderNames[2] = "4";
	orderNames[3] = "5";
	orderNames[4] = "6";
	orderNames[5] = "7";
	orderNames[6] = "8";
	orderNames[7] = "9";
	orderNames[8] = "10";
	orderNames[9] = "Boer";
	orderNames[10] = "Vrouw";
	orderNames[11] = "Heer";
	orderNames[12] = "Aas";
}

void humanPlay(){
	setCardNames();

	int playerNumber = 0;
	int cardsInformation[numberOfSuits][MAX_ROUNDS];
	int playerOrder[numberOfPlayers];
	int trickOrder[numberOfPlayers];
	int scoreBoard[numberOfPlayers]; // The total score
	int bidsMade[numberOfPlayers];
	int tricksMade[numberOfPlayers];

	string playerNames[numberOfPlayers];
	playerNames[playerNumber] = "CPU";

	int playedCards[numberOfPlayers][CARD_DIM];

	for(int i = 0; i < numberOfPlayers; i++){
		playerOrder[i] = i;
		scoreBoard[i] = 0;
		playedCards[i][0] = -1;
		playedCards[i][1] = -1;
		tricksMade[i] = 0;
		bidsMade[i] = 0;
		if(i > 0){
			cout << "Give the playername of player " << i << ". (On your left plus the number)" << endl;
			cin >> playerNames[i];
		}
	}
	cout << endl << "Which player is going to play the first card?" << endl;
	for(int i = 0; i < numberOfPlayers; i++){
		cout << i << ". " << playerNames[i] << endl;
	}
	cout << "Input the number left of the name: ";
	int temp;
	cin >> temp;
	cout << endl;
	while(playerOrder[0] != temp){
		shiftArray(playerOrder, numberOfPlayers);
	}
	for(int i = 0; i < numberOfPlayers; i++){
		trickOrder[i] = playerOrder[i];
	}

	//run the game!
	for(int round = numberOfRounds; round > 0; round--){
		for(int i = 0; i < numberOfSuits; i++){
			for(int j = 0; j < numberOfPlayers; j++){
				//cout << i << ", " << j << endl;
				suitPlayer[i][j] = false;
			}
		}
		for(int i = 0; i < numberOfSuits; i++){
			for(int j = 0; j < numberOfRounds; j++){
				cardsInformation[i][j] = -1;
			}
		}
		for(int i = 0; i < numberOfPlayers; i++){
			bidsMade[i] = 0;
			tricksMade[i] = 0;
			trickOrder[i] = playerOrder[i];
		}
		cout << "Enter card names as (SUIT)[ENTER] then (ORDERNAME)[ENTER]." << endl;
		if(round < numberOfRounds){
			cout << "Which card presented the trump suit? " << endl;
			int trumpSuit = -1;
			int trumpOrder = -1;
			getCardInput(trumpSuit, trumpOrder);
			while(cardsInformation[trumpSuit][trumpOrder] != -1){
				cout << "That card is already assigned, try again" << endl;
				getCardInput(trumpSuit, trumpOrder);
			}
			currentTrump = trumpSuit;
			cardsInformation[trumpSuit][trumpOrder] = -2;
			cout << endl;
		}
		cout << "Enter the input for the cards in your hands.." << endl;
		for(int cards = 0; cards < round; cards++){
			cout << "Requesting info on card " << cards+1 << endl;
			int selectedSuit = -1;
			int selectedOrder = -1;
			getCardInput(selectedSuit, selectedOrder);
			if(cardsInformation[selectedSuit][selectedOrder] != -1){
				//card is already taken
				cout << "That card is already taken, lets try again." << endl;
				cards--;
				continue;
			}
			else {
				string temp;
				cout << "If that card is correct type [Y]: ";
				cin >> temp;
				if(temp.compare("Y") == 0){
					break;
				}
				else {
					cout << "Lets retry entering that card " <<  cards+1 << ". " << endl;;
					cards--;
					continue;
				}
			}
			cardsInformation[selectedSuit][selectedOrder] = playerNumber;
		}
		//Get the bids
		int illegalBid = -1;
		int totalBid = 0;
		for(int i = 0; i < numberOfPlayers; i++){
			if(i+1 == numberOfPlayers){
				illegalBid = (round - totalBid);
				cout << "Last player can not bid " << illegalBid << endl;
			}
			if(playerOrder[i] == playerNumber){
				int pBid = playerControllerBid(2, playerOrder[i], round, illegalBid, cardsInformation, trickOrder);
				cout << "CPU bid: " << pBid << endl;
				bidsMade[playerOrder[i]] = pBid;
				totalBid += pBid;
			}
			else {
				cout << playerNames[playerOrder[i]] << "'s turn to bid now: ";
				int humanBid;
				cin >> humanBid;
				bool sure = false;
				while(!sure || humanBid == illegalBid){
					if(humanBid == illegalBid){
						cout << "That bid by " << playerNames[playerOrder[i]] << " is illegal, try again: ";
					}
					else {
						string temp;
						cout << "If that bid is correct type [Y]: ";
						cin >> temp;
						if(temp.compare("Y") == 0){
							break;
						}
						else {
							cout << "Enter the bid again for " <<  playerNames[playerOrder[i]] << ": ";
						}
					}
					cin >> humanBid;
				}
				bidsMade[playerOrder[i]] = humanBid;
				totalBid += humanBid;
			}
		}

		for(int trick = 0; trick < numberOfRounds; trick++){
			for(int p = 0; p < numberOfPlayers; p++){
				int suit = -1;
				int order = -1;
				if(trickOrder[p] == playerNumber){ //My turn to play
					cout << "I am going to play a card now:" << endl;
					int playableCards[numberOfRounds][CARD_DIM];
					for(int i = 0; i < numberOfRounds; i++){
						playableCards[i][0] = -1;
						playableCards[i][1] = -1;
					}
					for(int i = 0; i < numberOfPlayers; i++){
							cout << bidsMade[i] << endl;
							cout << tricksMade[i] << endl;
					}
					int temp = playerControllerPlay(3, playerNumber, round, scoreBoard, trickOrder, playedCards, playableCards, cardsInformation, bidsMade, tricksMade);
					cout << temp << endl;
					suit = playableCards[temp][0];
					order = playableCards[temp][1];
					cout << suit << ", " << order << endl;
					cout << "I play: " << suitNames[suit] << " " << orderNames[order] << endl;
				}
				else { //Human turn
					cout << playerNames[trickOrder[p]] << " is going to play a card now:" << endl;
					getCardInput(suit, order);
					string t;
				}
				playSimulatedMove(trickOrder[p], suit, order, cardsInformation, playedCards, suitPlayer);
			}
			int winner = trickOrder[trickWinner(playedCards)];
			cout << playerNames[winner] << " won the trick!" << endl;
			tricksMade[winner]++;
			while(trickOrder[0] != winner){
				shiftArray(trickOrder, numberOfPlayers);
			}
			for(int i = 0; i < numberOfPlayers; i++){
				playedCards[i][0] = -1;
				playedCards[i][1] = -1;
			}
		}
		cout << "Done with round " << round << endl;
		printRoundScore(round, bidsMade, tricksMade);
		for (int i = 0; i < numberOfPlayers; i++){
			if(bidsMade[i] == tricksMade[i]){ //won as much as you bid, so score += x + y*c
				scoreBoard[i] += (WIN_CONST + tricksMade[i]*WIN_FACT); //room for factoring
			}
			else { //over or under shot, so score -= abs(x - y)*c
				scoreBoard[i] -= (abs(bidsMade[i] - tricksMade[i])*LOSE_FACT); //room for factoring
			}
		}
		int abVic = absoluteVictor(scoreBoard, round-1);
		if(abVic == 0){ //do stuff, there is an absolute victor
			cout << "There is an absolute victor: " << playerNames[abVic] << " can no longer be passed by anyone and has won the game!" << endl;
			break; //break out of the rounds
		}
		shiftArray(playerOrder, numberOfPlayers);
	}
	totalPPoints[0] += scoreBoard[0];
	int pWin = gameWinner(scoreBoard); //which player has won?
	printScoreBoard(scoreBoard);

	cout << endl << "!------------------------------------------!" << playerNames[pWin] << " has won the game!" << endl << "!------------------------------------------!" << endl;
}

void menu(){
	int opt;

	for(int i = 0; i < numberOfPlayers; i++){
		algSelection[i][0] = 1;
		algSelection[i][1] = 0;
	}
	algSelection[0][0] = 1;
	algSelection[0][1] = 3;

  cout << endl << "----------------------------" << endl;
  cout << endl << "      Main Menu   " << endl;
  cout << endl << "----------------------------" << endl;

	//print current settings

	cout << endl << "			Select your choice:		" << endl;
	cout << "			1. Change bidding algorithms" << endl;
	cout << "			2. Change playing algorithms" << endl;
	cout << "			3. Change number of games" << endl;
	cout << "			4. Run simulation" << endl;
	cout << "			5. Play against three humans using MCTS" << endl;
	cout << "			0. EXIT PROGRAM" << endl;

  cin >> opt;
  cout << "\n" << endl;

  switch (opt)
  {
      case 0:
          exit(0);
          break;
      case 1:
          //Code
          break;
      case 2:
          break;
			case 3:
					break;
			case 4:
					break;
			case 5:
					break;
      default:
          break;
  }

}

int main(int argc, char *argv[]){
	agentNames[0] = "Random Agent";
	agentNames[1] = "Trumping Agent";
	agentNames[2] = "Rule Based Agent";
	agentNames[3] = "Monte Carlo Agent";
	agentNames[4] = "Monte Carlo Tree Search Agent";
	//random agent setting
	agentSettings[0][0] = 1;
	agentSettings[0][1] = 0;
	//trumping agent settings
	agentSettings[1][0] = 1;
	agentSettings[1][1] = 1;
	//rule based agent settings
	agentSettings[2][0] = 1;
	agentSettings[2][1] = 2;
	//MC agent settings
	agentSettings[3][0] = 2;
	agentSettings[3][1] = 3;
	//MCTS agent settings
	agentSettings[4][0] = 2;
	agentSettings[4][1] = 4;

	//agents
	agentSelection[0] = 3;
	agentSelection[1] = 0;
	agentSelection[2] = 0;
	agentSelection[3] = 0;

	srand(time(NULL));
	signal(SIGINT, signalHandler);
	start = clock();
	mcBudget = 10000;
	if(atoi(argv[1]) == -1){ //play against humans
		numberOfPlayers = 4;
		numberOfRounds = 13;
		numberOfSuits = 4;
		numberOfCards = 52;
		orderSize = 13;
		humanPlay();
		return 1;
	}
	if(argc < 3){
		cout << "Error, invalid arguments. Required: 3. Form: #PLAYERS(INT) #ROUNDS(INT) #SUITS(INT) #GAMES(INT) #BUDGET(INT)." << endl;
		cout << "Exiting now." << endl;
		return -1;
	}

	//initialize player parameters
	numberOfPlayers = atoi(argv[1]);
	numberOfRounds = atoi(argv[2]);
	numberOfSuits = atoi(argv[3]);
	numberOfGames = atoi(argv[4]);
	mcBudget = atoi(argv[5]);

	numberOfCards = numberOfPlayers * numberOfRounds;
	orderSize = numberOfCards / numberOfSuits;
	if(numberOfCards % numberOfSuits > 0){
		cout << "Error 01: Number of Types not a mod factor of cards." << endl;
		return -1;
	}
	if (MIN_PLAYERS > numberOfPlayers || MAX_PLAYERS < numberOfPlayers){
		cout << "Error 02: Number of players invalid, out of range:(" << MIN_PLAYERS << ", " << MAX_PLAYERS << ")." << endl;
		return -1;
	}
	if (MIN_ROUNDS > numberOfRounds || MAX_ROUNDS < numberOfRounds){
		cout << "Error 03: Number of rounds invalid, out of range:(" << MIN_ROUNDS << ", " << MAX_ROUNDS << ")." << endl;
		return -1;
	}
	if (numberOfGames < 1 || numberOfGames > __INT_MAX__){
		cout << "Error 04: Number of games invalid, out of range:(" << MIN_GAMES << ", " << __INT_MAX__ << ")." << endl;
	}
	if (numberOfSuits > MAX_PLAYERS || numberOfSuits < MIN_PLAYERS){
		cout << "Error 05: Number of suits invalid, out of range:(" << MIN_PLAYERS << ", " << MAX_PLAYERS << ")." << endl;
	}
	if(mcBudget <= 1){
		cout << "Error 06: Invalid Budget: " << mcBudget << endl;
	}

	//Variables about the algoritm selection, index 0 is the bidding algorithm, index 1 is the playing algorithm
	//int algSelection[numberOfPlayers][2];

	for(int i = 0; i < numberOfPlayers; i++){
		algSelection[i][0] = agentSettings[agentSelection[i]][0];
		algSelection[i][1] = agentSettings[agentSelection[i]][1];
	}
	// algSelection[0][0] = 2;
	// algSelection[0][1] = 3;

	//Variables about the entire game
	int cardsArray[numberOfSuits][MAX_ROUNDS]; //initialize 2D array of all the cards
	int scoreBoard[numberOfPlayers]; // The total score
	int playerOrder[numberOfPlayers]; //The order in which player may place their cards

	//Variables inside a round
	int trickCards[numberOfPlayers][CARD_DIM]; //array of trick, [i][0] is the suit, [i][1] is the number
	int trickWins[numberOfPlayers]; //how many tricks each player has won
	int trickOrder[numberOfPlayers]; //The order within a round changes
	int trickBids[numberOfPlayers]; //How many each player has bid to win

	//possible moves array
	int posmov[numberOfRounds][CARD_DIM];
	for(int i = 0; i < numberOfRounds; i++){
		posmov[i][0] = -1;
		posmov[i][1] = -1;
	}
	//Start the game!
	cout << "Running with " << mcBudget << " budget and " << MCTS_CONSTANT << " exploration factor." << endl;
	cout << "Agents: " << endl;
	for(int i = 0; i < numberOfPlayers; i++){
		cout << i << ". " << agentNames[agentSelection[i]] << endl;
	}
	cout << endl;
	for (int games = 0; games < numberOfGames; games++){
		initializeGame(scoreBoard, trickWins, playerOrder);
		if(games%(numberOfGames/10) == 0){
			duration = (clock() - start) / ((double) CLOCKS_PER_SEC);
			cout << "...." << games << "/" << numberOfGames << "... (" << (int)(duration/3600) << "h " << (((int)duration/60)%60) << "m " << ((int)duration%60) << "s)" << endl;
		}

		for (int playRound = numberOfRounds; playRound > 0; playRound--){
			initializeRound(trickWins, playRound, cardsArray, trickCards, playerOrder, trickOrder, suitPlayer);
			int totalBids = 0;
			int illegalBid = -1;
			for (int i = 0; i < numberOfPlayers; i++){//set up the bids
				if(i+1 == numberOfPlayers){ //last bidder, fuck the dealer rule
					illegalBid = totalBids - playRound;
				}
				trickBids[playerOrder[i]] = playerControllerBid(algSelection[playerOrder[i]][0], playerOrder[i], playRound, illegalBid, cardsArray, trickOrder);
				totalBids += trickBids[playerOrder[i]];
			}

			for(int numTricks = 0; numTricks < playRound; numTricks++){ //play all tricks in the round
				for(int a = 0; a < numberOfPlayers; a++){ //play a trick
					int moves = 0;
					int move = -1;
					possibleMoves(posmov, cardsArray, moves, trickCards, trickOrder[a], playRound);
					if(moves == 1){ //if there is only one legal move, no need to consult the agent; play it immediatly
						move = 0;
					}
					else {
						move = playerControllerPlay(algSelection[trickOrder[a]][1], trickOrder[a], playRound, scoreBoard, trickOrder, trickCards, posmov, cardsArray, trickBids, trickWins);
					}

					if (!playMove(trickOrder[a], posmov[move][0], posmov[move][1], cardsArray, trickCards)){
						exit(-1); //Something went very wrong
					}
					for(int i = 0; i < playRound; i++){ //move played, clear the possible moves
						posmov[i][0] = -1;
						posmov[i][1] = -1;
					}
				}
				int winner = trickOrder[trickWinner(trickCards)];
				trickWins[winner]++;
				while(trickOrder[0] != winner){ //trick winner starts next trick
					shiftArray(trickOrder, numberOfPlayers);
				}
				for(int i = 0; i < numberOfPlayers; i++){ //reset the cards on the table
					trickCards[i][0] = -1;
					trickCards[i][1] = -1;
				}
			}
			for (int i = 0; i < numberOfPlayers; i++){
				if(trickBids[i] == trickWins[i]){ //won as much as you bid, so score += x + y*c
					scoreBoard[i] += (WIN_CONST + trickWins[i]*WIN_FACT); //room for factoring
				}
				else { //over or under shot, so score -= abs(x - y)*c
					scoreBoard[i] -= (abs(trickBids[i] - trickWins[i])*LOSE_FACT); //room for factoring
				}
			}
			int abVic = absoluteVictor(scoreBoard, playRound-1);
			if(abVic >= 0){ //do stuff, there is an absolute victor
				absoluteVictorCounter[abVic]++;
				break; //break out of the rounds
			}
		}

		int pWin = gameWinner(scoreBoard); //which player has won?
		for (int i = 0; i < numberOfPlayers; i++){
			totalPPoints[i] += scoreBoard[i];
		 	if (scoreBoard[i] > 0){
		 		totalPoints += scoreBoard[i];
		 	}
		}
		totalPwins[pWin]++;
		// if(pWin == 0){
		// 	totalPwins++;
		// }
		tempGames++;
	}
	duration = ( clock() - start ) / (double) CLOCKS_PER_SEC;
	cout << "Total scoreboard points(" << tempGames << " games): "<< totalPoints << endl;;
	for(int i = 0; i < numberOfPlayers; i++){
		cout << "Player " << i << "(" << agentNames[agentSelection[i]] << "): " << endl;
		cout << "Total points: " << totalPPoints[i] << endl;
		cout << "Player share: " << (double)totalPPoints[i]/totalPoints *100.0  << "%" << endl;
		cout << "Player wins: " << (double)totalPwins[i]/tempGames * 100.0 << "%" << endl;
		cout << "Number of absolute victories: " << absoluteVictorCounter[i] << endl;
	}
	cout << "Duration of calculation: " << (int)(duration/3600) << "h " << (((int)duration/60)%60) << "m " << ((int)duration%60) << "s" << endl;
	return 0;
}
