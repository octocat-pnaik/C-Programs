/*
* Developer: Purnima Naik
* Summary: Program to implement to implement a simple blockchain.
*/

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES 7
#define MAXIMUM_NUMBER_OF_ALLOWED_TRANSACTIONS 12

// Declarations
struct blockChainNode
{
    int nodeId;
	int totalBitcoins;
	int nodeAdded;
};

struct transaction
{
    int transactionId;
    int senderId;
    int receiverId;
    int senderPreviousAmount;
    int senderBalanceAmount;
    int receiverPreviousAmount;
    int receiverBalanceAmount;
    int transactionAmount;
    int transactionAdded;
};

// Datatype of global & local ledger
// The data for the block chain nodes and a list of transactions that were successful will be kept in the ledger.
struct blockChain
{
    struct blockChainNode nodes[MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES];
    struct transaction transactions[MAXIMUM_NUMBER_OF_ALLOWED_TRANSACTIONS];
};

static struct blockChain _blockChainData;

enum receiverTransactionValidation
{
    VALID = 1,
    INVALID = 2
};

enum transactionStatus
{
    ACCEPTED = 1,
    DECLINED = 2
};

int _senderId, _receiverId, _transactionCounter, _countOfVotes, _loanAmount, _transactionId;
int _countOfReaders, _countOfWriters;

// If the value of _receiverTransactionValidity is 1, it means the transaction is confirmed as valid by the receiver
// If the value of _receiverTransactionValidity is 2, it means the transaction is confirmed as invalid by the receiver
int _receiverTransactionValidity;

// If the value of _transactionStatus is 1, then it means the transaction is accepted and processed by the receiver.
// If the value of _transactionStatus is 2, then it means the transaction is declined by the receiver
int _transactionStatus;

sem_t readerLock, writerLock;
pthread_mutex_t transactionLock;
pthread_cond_t newTransaction, validateTransaction, transactionStatus, transactionValidationStatusByReceiver, validatedTransaction;


/*
* Summary - This method will check if the node data exists in the global ledger.
* Param 1 - nodeId - the thread id
* Returns 1, if the node exists, else returns 0.
*/
int nodeAlreadyExistsInGlobalLedger(int nodeId)
{
    // Declarations
    int counter;

    for(counter = 0; counter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; counter++)
    {
        if(_blockChainData.nodes[counter].nodeId == nodeId)
        {
            return 1;
        }
    }

    return 0;
}

/*
* Summary - This method will add the node's data in the global ledger.
* Param 1 - nodeId - the thread id
* Param 2 - totalAmount - the node's initial balance
*/
void addBlockchainNodeInGlobalLedger(int nodeId, int totalAmount)
{
    // Declarations
    int counter = 0;

    while(counter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES)
    {
        if(_blockChainData.nodes[counter].nodeAdded == 0)
        {
            _blockChainData.nodes[counter].nodeId = nodeId;
            _blockChainData.nodes[counter].totalBitcoins = totalAmount;
            _blockChainData.nodes[counter].nodeAdded = 1;
            break;
        }
        counter++;
    }
}

/*
* Summary - This method will populate the local ledger of every node with the global ledger's _blockChainData data.
* Param 1 - ledger - the local ledger of every node which is of type struct blockChain
* Returns populated local ledger
*/
struct blockChain addNodesDataInLocalLedger(struct blockChain ledger)
{
    // Declarations
    int counter = 0;

    for(counter = 0; counter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; counter ++)
    {
        ledger.nodes[counter].nodeId = _blockChainData.nodes[counter].nodeId;
        ledger.nodes[counter].totalBitcoins = _blockChainData.nodes[counter].totalBitcoins;
        ledger.nodes[counter].nodeAdded = 1;
    }

    return ledger;
}

/*
* Summary - This method will be called to update the local and global ledgers after a successful transaction.
* A new transaction will be added, and the new amount of the node will be updated.
* Param 1 - ledger - the local of global ledger of type struct blockChain
* Param 2 - senderId - sender id
* Param 3 - receiverId - receiver id
* Param 4 - skipSenderBalanceUpdate - if 1, do not calculate the sender's new balance amount.
* Returns updated ledger
*/
struct blockChain updateTransactionDataInLedger(struct blockChain ledger, int senderId, int receiverId, int skipSenderBalanceUpdate)
{
    // Declarations
    int loopCounter = 0, counter = 0, senderPrevAmount, receiverPrevAmount, senderBalance, receiverBalance;

    // Update sender and receiver balance in the ledger
    for(loopCounter = 0; loopCounter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; loopCounter ++)
    {
        if(ledger.nodes[loopCounter].nodeId == senderId)
        {
            if(skipSenderBalanceUpdate == 1)
            {
                senderBalance = ledger.nodes[loopCounter].totalBitcoins;
                senderPrevAmount = senderBalance + _loanAmount;
            }
            else
            {
                senderPrevAmount = ledger.nodes[loopCounter].totalBitcoins;
                senderBalance = senderPrevAmount - _loanAmount;
                ledger.nodes[loopCounter].totalBitcoins = senderBalance;
            }
        }

        if(ledger.nodes[loopCounter].nodeId == receiverId)
        {
            receiverPrevAmount = ledger.nodes[loopCounter].totalBitcoins;
            receiverBalance = receiverPrevAmount + _loanAmount;
            ledger.nodes[loopCounter].totalBitcoins = receiverBalance;
        }
    }

    // Add a new transaction
    while(counter < MAXIMUM_NUMBER_OF_ALLOWED_TRANSACTIONS)
    {
        if(ledger.transactions[counter].transactionAdded == 0)
        {
            ledger.transactions[counter].transactionId = _transactionId;
            ledger.transactions[counter].senderId = senderId;
            ledger.transactions[counter].receiverId = receiverId;
            ledger.transactions[counter].senderPreviousAmount = senderPrevAmount;
            ledger.transactions[counter].senderBalanceAmount = senderBalance;
            ledger.transactions[counter].receiverBalanceAmount = receiverBalance;
            ledger.transactions[counter].receiverPreviousAmount = receiverPrevAmount;
            ledger.transactions[counter].transactionAmount = _loanAmount;
            ledger.transactions[counter].transactionAdded = 1;
            break;
        }
        counter++;
    }

    return ledger;
}

/*
* Summary - This method will block the nodes from reading the shared data while it is getting updated.
*/
void blockReaders()
{
    _countOfWriters = _countOfWriters + 1;
    if(_countOfWriters == 1)
    {
        sem_wait(&readerLock);
    }
}

/*
* Summary - Once the nodes are done updating the shared data,
* this method will allow the waiting nodes to read the shared data.
*/
void allowReaders()
{
    if(_countOfWriters == MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES)
    {
        _countOfWriters = 0;
        sem_post(&readerLock);
    }
}

/*
* Summary - This method will block the nodes from updating the shared data while it is getting read.
*/
void blockWriters()
{
    _countOfReaders = _countOfReaders + 1;
    if(_countOfReaders == 1)
    {
        sem_wait(&writerLock);
    }
}

/*
* Summary - Once the nodes are done reading the shared data,
* this method will allow the waiting writers to update the shared data.
*/
void allowWriters()
{
    if(_countOfReaders == MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES)
    {
        _countOfReaders = 0;
        sem_post(&writerLock);
    }
}

/*
* Summary - This method will check if the sender node is legitimate.
* It determines its legitimacy by ascertaining whether the sender genuinely possesses the sum he intends to lend the recipient.
* Param 1 - ledger - the local ledger of every node which is of type struct blockChain
* Returns 1, if the sender is valid, else 0.
*/
int isSenderNodeLegitimate(struct blockChain ledger)
{
    // Declarations
    int loopCounter = 0;

    for(loopCounter = 0; loopCounter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; loopCounter ++)
    {
        if(ledger.nodes[loopCounter].nodeId == _senderId)
        {
            int senderAmount = ledger.nodes[loopCounter].totalBitcoins;
            int senderBalance = senderAmount - _loanAmount;
            if(senderBalance >= 0)
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }

    return 1;
}

/*
* Summary - This function is executed by each thread or block chain node.
* The thread will alternately act as a sender, receiver, or validator on each run.
* Following the assignment of duties to each thread, they will all carry out a loan transaction
* in which the sender thread lends money to the receiver thread, and the validator threads verify the transaction.
* Param 1 - argumentsData - the thread arguments data
*/
void *blockChainNode(void *argumentsData)
{
    // Declarations
    int nodeId = 0, loopCounter = 0, senderId = 0, receiverId = 0;
    struct blockChain localBlockChainData;
    int maxNodes = MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES;
    bool isSender = false, isReceiver = false, isValidator = false;

    // Step 1 - Get the node id
    nodeId = (*((int *)argumentsData));

    while(1)
    {
        sem_wait(&readerLock);
        blockWriters();

        // Step 2 - The program is made to carry out transactions up to the MAXIMUM_NUMBER_OF_ALLOWED_TRANSACTIONS.
        // So verify how many transactions we have actually completed.
        // Terminate the threads if we have reached that number.
        if(_transactionCounter == MAXIMUM_NUMBER_OF_ALLOWED_TRANSACTIONS)
        {
            sem_post(&readerLock);
            break;
        }

        allowWriters();
        sem_post(&readerLock);

        sem_wait(&writerLock);
        blockReaders();

        // Step 3 - Generate a positive random amount for each node, and add each node to the global ledger if it is not already there.
        int isNodeAddedToGlobalLedger = nodeAlreadyExistsInGlobalLedger(nodeId);
        if(isNodeAddedToGlobalLedger == 0)
        {
            int initialAmount;
            while(initialAmount <= 0)
            {
                initialAmount = rand() % 20;
            }
            addBlockchainNodeInGlobalLedger(nodeId, initialAmount);
        }

        allowReaders();
        sem_post(&writerLock);

        sem_wait(&readerLock);
        blockWriters();

        // Step 4 - Update each node's local ledger with the global data after adding each node to the global ledger.
        if(localBlockChainData.nodes[maxNodes - 1].nodeAdded != 1)
        {
            localBlockChainData = addNodesDataInLocalLedger(localBlockChainData);
        }

        allowWriters();
        sem_post(&readerLock);

        sem_wait(&writerLock);

        // Step 5 - In its iteration, each thread will either take on the role of a sender, receiver, or validator.
        if(_senderId == 0)
        {
            _senderId = nodeId;

            // Assign data to thread's local variables
            senderId = _senderId;
            isSender = true;

            // Before initiating a new transaction, reset the global data
            _loanAmount = 0;
            _receiverTransactionValidity = 0;
            _transactionStatus = 0;
            _transactionId = 0;
            _countOfVotes = 0;
        }
        else if(_receiverId == 0)
        {
            _receiverId = nodeId;

            // Assign data to thread's local variables
            isReceiver = true;
            senderId = _senderId;
            receiverId = _receiverId;
        }
        else
        {
            // Assign data to thread's local variables
            isValidator = true;
            senderId = _senderId;
            receiverId = _receiverId;
        }

        sem_post(&writerLock);

        // Step 6 - Once the roles are assigned to the threads,
        // they start a loan transaction in which the sender sends bitcoins to the receiver,
        // the validator and the receiver verifies the transaction, and then the receiver accepts or declines the transaction.
        // If the transaction is accepted by the receiver, the receiver writes transaction data in its local and global ledger,
        // and notifies the other threads to update their local ledger with the transaction data.
        if(isSender)
        {
            // Displays participating nodes and their initial amount from its local ledger.
            printf("\n--------------Participating Nodes-------------\n");
            printf("\033[0;36m");
            printf("Node Id      Amount\n");
            printf("\033[0m");
            for(loopCounter = 0; loopCounter < maxNodes; loopCounter++)
            {
                printf("%d            %d\n", localBlockChainData.nodes[loopCounter].nodeId, localBlockChainData.nodes[loopCounter].totalBitcoins);
            }

            sleep(1);

            pthread_mutex_lock(&transactionLock);

            // Sender acquires the transaction lock and generates a transaction id
            _transactionCounter = _transactionCounter + 1;
            _transactionId = _transactionCounter;

            // Generates a random loan amount
            int loanAmount;
            while(loanAmount <= 0)
            {
                // If the sender's thread ID is 2, it will make a loan amount that is not valid.
                // This condition is added to get a failed transaction.
                if(nodeId == 2)
                {
                    loanAmount = rand() % 5 + 100;
                }
                else
                {
                    loanAmount = rand() % 5;
                }
            }

            // Deduct loan amount from the sender's amount in his local ledger
            int currentAmount, remainingAmount;
            for(loopCounter = 0; loopCounter < maxNodes; loopCounter ++)
            {
                if(localBlockChainData.nodes[loopCounter].nodeId == nodeId)
                {
                    currentAmount = localBlockChainData.nodes[loopCounter].totalBitcoins;
                    remainingAmount = currentAmount - loanAmount;
                    localBlockChainData.nodes[loopCounter].totalBitcoins = remainingAmount;
                    break;
                }
            }

            printf("\033[0;33m");
            printf("\n------------Starting Transaction %d------------\n", _transactionId);
            printf("\033[0m");

            printf("Transaction Id: ");
            printf("\033[0;36m");
            printf("%d", _transactionId);
            printf("\033[0m");

            printf("\nSender Node Id: ");
            printf("\033[0;36m");
            printf("%d", nodeId);
            printf("\033[0m");

            printf("\nSender Initial Amount: ");
            printf("\033[0;36m");
            printf("%d", currentAmount);
            printf("\033[0m");

            printf("\nTransaction Amount: ");
            printf("\033[0;36m");
            printf("%d", loanAmount);
            printf("\033[0m");

            _loanAmount = loanAmount;
            receiverId = _receiverId;

            // The sender informs the receiver about this new transaction.
            pthread_cond_broadcast(&newTransaction);

            // The sender waits for a transaction status from the receiver.
            // As the sender is waiting on a condition, it will release the transactionLock.
            // So that the waiting receiver, can proceed its execution with the help of this lock.
            while(_transactionStatus == 0)
            {
                pthread_cond_wait(&transactionStatus, &transactionLock);
            }

            // If receiver declines the transaction, then rollback the transaction.
            // Revert the changes performed above in the sender's local ledger
            if(_transactionStatus == DECLINED)
            {
                for(loopCounter = 0; loopCounter < maxNodes; loopCounter ++)
                {
                    if(localBlockChainData.nodes[loopCounter].nodeId == nodeId)
                    {
                        localBlockChainData.nodes[loopCounter].totalBitcoins = remainingAmount + loanAmount;
                        break;
                    }
                }
            }

            // If receiver accepts the transaction, then update the local ledger.
            // Add new transaction details in the local ledger.
            if(_transactionStatus == ACCEPTED)
            {
                // Update the local ledger
                localBlockChainData = updateTransactionDataInLedger(localBlockChainData, senderId, receiverId, 1);
            }

             // Reset the global and local data
            _senderId = 0;
            isSender = false;
            senderId = 0;
            receiverId = 0;

            pthread_mutex_unlock(&transactionLock);
        }

        if(isReceiver)
        {
            pthread_mutex_lock(&transactionLock);

            // The receiver waits for a signal from the sender that a new transaction has started.
            // Once it gets the signal from the sender, the sender waits for a signal from the receiver about the status of the transaction,
            // and the receiver starts validating the transaction.
            while(_loanAmount == 0)
            {
                pthread_cond_wait(&newTransaction, &transactionLock);
            }

            printf("\nReceiver Node Id: ");
            printf("\033[0;36m");
            printf("%d", nodeId);
            printf("\033[0m");

            int receiverInitialAmount;
            for(loopCounter = 0; loopCounter < maxNodes; loopCounter ++)
            {
                if(localBlockChainData.nodes[loopCounter].nodeId == nodeId)
                {
                    receiverInitialAmount = localBlockChainData.nodes[loopCounter].totalBitcoins;
                    break;
                }
            }

            printf("\nReceiver Initial Amount: ");
            printf("\033[0;36m");
            printf("%d", receiverInitialAmount);
            printf("\033[0m");

            // Validate the sender by using the localBlockChainData
            int senderNodeLegitimate = isSenderNodeLegitimate(localBlockChainData);

            if(senderNodeLegitimate == 1)
            {
                // The sender is a valid node.
                printf("\033[0;32m");
                printf("\nThe receiver validated that the transaction is legitimate.");
                printf("\nBroadcasting a request to other nodes to verify this transaction.\n");
                printf("\033[0m");

                // Inform the waiting validators, that the sender is valid.
                // So that the validators, can validate the transaction.
                _receiverTransactionValidity = VALID;
                pthread_cond_broadcast(&transactionValidationStatusByReceiver);

                // As the sender is valid, the receiver waits to get transaction validation status response
                // from all the validators.
                int countOfValidators = maxNodes - 2;
                while(countOfValidators != _countOfVotes)
                {
                    pthread_cond_wait(&validatedTransaction, &transactionLock);
                }

                // Once the transaction is validated by the validators, the receiver updates the transaction details in the local ledger.
                localBlockChainData = updateTransactionDataInLedger(localBlockChainData, senderId, receiverId, 0);
                // Then the receiver, updates the transaction details in the global ledger.
                _blockChainData = updateTransactionDataInLedger(_blockChainData, senderId, receiverId, 0);

                int sendBalAmt, recBalAmt;
                for(loopCounter = 0; loopCounter < maxNodes; loopCounter ++)
                {
                    if(localBlockChainData.nodes[loopCounter].nodeId == senderId)
                    {
                        sendBalAmt = localBlockChainData.nodes[loopCounter].totalBitcoins;
                    }

                    if(localBlockChainData.nodes[loopCounter].nodeId == receiverId)
                    {
                        recBalAmt = localBlockChainData.nodes[loopCounter].totalBitcoins;
                    }
                }

                printf("Receiver Balance Amount: ");
                printf("\033[0;36m");
                printf("%d", recBalAmt);
                printf("\033[0m");

                printf("\nSender Balance Amount: ");
                printf("\033[0;36m");
                printf("%d", sendBalAmt);
                printf("\033[0m");

                printf("\033[0;33m");
                printf("\n------------------SUCCESS---------------------\n");
                printf("\033[0m");

                // The sender informs the waiting sender, and the waiting validators that the transaction is valid,
                // and hence it is accepted and processed by the receiver.
                _transactionStatus = ACCEPTED;
                pthread_cond_broadcast(&transactionStatus);

                // Reset the data
                isReceiver = false;
                _receiverId = 0;
                senderId = 0;
                receiverId = 0;

                pthread_mutex_unlock(&transactionLock);
            }
            else
            {
                // It will come here, if the sender node is invalid.
                printf("\033[0;31m");
                printf("\nThe recipient has verified that this is a fraudulent transaction and has consequently denied the bitcoins.");
                printf("\033[0m");
                printf("\033[0;33m");
                printf("\n------------------FAILED---------------------\n");
                printf("\033[0m");

                // The receiver informs the waiting validators, that the transaction is invalid.
                _receiverTransactionValidity = INVALID;
                pthread_cond_broadcast(&transactionValidationStatusByReceiver);

                // The receiver informs the waiting sender that the transaction is declined.
                _transactionStatus = DECLINED;
                pthread_cond_broadcast(&transactionStatus);

                // Reset the local and global data
                isReceiver = false;
                _receiverId = 0;
                senderId = 0;
                receiverId = 0;

                pthread_mutex_unlock(&transactionLock);
            }
        }

        if(isValidator)
        {
            pthread_mutex_lock(&transactionLock);

            // When a thread will become a validator, it will just wait to get the transaction validation status from the receiver.
            // If the transaction validation status is INVALID, it means the transaction is declined by the receiver,
            // and the validator just needs to reset the data and participate in another transaction.
            while(_receiverTransactionValidity == 0)
            {
                pthread_cond_wait(&transactionValidationStatusByReceiver, &transactionLock);
            }

            // If the transaction validation status is VALID, the validator will validate the sender,
            // and will signal the receiver, that it has completed its validation.
            if(_receiverTransactionValidity == VALID)
            {
                // Validate the sender
                int senderNodeLegitimate = isSenderNodeLegitimate(localBlockChainData);
                if(senderNodeLegitimate == 1)
                {
                    _countOfVotes = _countOfVotes + 1;
                    printf("\033[0;32m");
                    printf("+ ");
                    printf("\033[0m");
                    printf("Node %d confirmed the transaction as valid.\n", nodeId);

                    // Signal the receiver that trasaction is validated and it is valid.
                    pthread_cond_signal(&validatedTransaction);

                    // Wait for the transaction status from the receiver.
                    while(_transactionStatus == 0)
                    {
                        pthread_cond_wait(&transactionStatus, &transactionLock);
                    }

                    // If the trasaction is accepted and processed, then update the local ledger with the transaction details
                    if(_transactionStatus == ACCEPTED)
                    {
                        // Update the local ledger
                        localBlockChainData = updateTransactionDataInLedger(localBlockChainData, senderId, receiverId, 0);
                    }
                }
            }

            pthread_mutex_unlock(&transactionLock);

            // Reset the local data
            isValidator = false;
            senderId = 0;
            receiverId = 0;
        }
    }

    pthread_exit(0);
}

int main()
{
    // Declarations
    int counter;
    pthread_t participatingNodes[MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES];
    int nodeIds[MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES];

    srand(time(NULL));

    // Initialize the mutex, conditional variable, semaphore
    pthread_cond_init(&newTransaction, NULL);
    pthread_cond_init(&validateTransaction, NULL);
    pthread_cond_init(&transactionValidationStatusByReceiver, NULL);
    pthread_cond_init(&transactionStatus, NULL);
    pthread_cond_init(&validatedTransaction, NULL);
    pthread_mutex_init(&transactionLock, NULL);
    sem_init(&readerLock, 0, 1);
    sem_init(&writerLock, 0, 1);

    for(counter = 0; counter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; counter++)
    {
        nodeIds[counter] = counter + 1;
    }

    for(counter = 0; counter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; counter++)
    {
        pthread_create(&participatingNodes[counter], NULL, (void *)blockChainNode, (void *)&nodeIds[counter]);
    }

    for(counter = 0; counter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; counter++)
    {
        pthread_join(participatingNodes[counter], NULL);
    }

    // Destroy the mutex, conditional variable, semaphore
    sem_destroy(&readerLock);
    sem_destroy(&writerLock);

    pthread_mutex_destroy(&transactionLock);
    pthread_cond_destroy(&newTransaction);
    pthread_cond_destroy(&validateTransaction);
    pthread_cond_destroy(&transactionValidationStatusByReceiver);
    pthread_cond_destroy(&transactionStatus);
    pthread_cond_destroy(&validatedTransaction);

    printf("\033[0;35m");
    printf("\n----------- ALL TRANSACTIONS ARE EXECUTED -----------\n");
    printf("\033[0m");

    printf("\n******Block Chain Data From the Global Ledger******\n");

    printf("\n------------Participating Nodes------------\n");
    printf("\033[0;36m");
    printf("Node Id      Amount\n");
    printf("\033[0m");

    int loopCounter;
    for(loopCounter = 0; loopCounter < MAXIMUM_NUMBER_OF_BLOCKCHAIN_NODES; loopCounter++)
    {
        printf("%d            %d\n", _blockChainData.nodes[loopCounter].nodeId, _blockChainData.nodes[loopCounter].totalBitcoins);
    }

    printf("\n------------Transaction Data------------\n");
    printf("\033[0;36m");
    printf("Id - SId - RId - SPrevAmt - TransAmt - SBalAmt - RPrevAmt - RBalAmt\n");
    printf("\033[0m");

    for(loopCounter = 0; loopCounter < MAXIMUM_NUMBER_OF_ALLOWED_TRANSACTIONS; loopCounter++)
    {
        if(_blockChainData.transactions[loopCounter].transactionAdded == 1)
        {
            printf("%d - %d - %d - %d - %d - %d - %d - %d\n",
            _blockChainData.transactions[loopCounter].transactionId,
            _blockChainData.transactions[loopCounter].senderId,
            _blockChainData.transactions[loopCounter].receiverId,
            _blockChainData.transactions[loopCounter].senderPreviousAmount,
            _blockChainData.transactions[loopCounter].transactionAmount,
            _blockChainData.transactions[loopCounter].senderBalanceAmount,
            _blockChainData.transactions[loopCounter].receiverPreviousAmount,
            _blockChainData.transactions[loopCounter].receiverBalanceAmount);
        }
    }

    return 0;
}

