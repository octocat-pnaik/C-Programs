###########PROGRAM DESIGN###########
The node that is the thread in a blockchain transaction may be a sender sending bitcoins, a
receiver receiving bitcoins, or a validator who validates the transaction. In addition, each node
stores all node data and transaction data in its local ledger. Keeping this in mind, the following
program design is presented.

-----Structure of Blockchain Node (Thread)-----
1. Add its details, including the node id and starting balance, to the global ledger.
2. Update the local ledger with all participant node data, including node id and starting
balance for each node with the help of the global ledger.
3. Become a sender, receiver or a validator.
4. If sender:
- Create a random amount to loan to the receiver.
- Begin a transaction by creating a transaction id.
- Deduct loan amount from the sender's amount in his local ledger.
- Send loan amount and transaction details to the receiver.
- Wait for an acknowledgment from the receiver.
- If a transaction is accepted, add transaction details and update node balances in local
ledger.
- If a transaction is declined, rollback the changes performed in the local ledger.
- Resets the used variables to participate in the next transaction.
5. If receiver:
- The receiver waits for a signal from the sender that a new transaction has started.
- Once it receives the notification from the sender, it starts validating the sender against
the data present in its local ledger.
- If the sender is legitimate, it asks the validators to confirm the transaction and waits
until it hears back from all of them.
- Once it gets confirmation from all the validators, it writes transaction details and
updates node’s balances in its local ledger and the global ledger.
- After writing the transaction details, it informs the waiting sender and waiting validators
that the transaction is accepted and processed.
- Resets the used variables to participate in the next transaction.
6. If validator:
- The validator waits to get the transaction validation status from the receiver.
- If the transaction validation status is INVALID, it means the transaction is declined by
the receiver, and the validator just needs to reset the data to participate in another
transaction.
- If the transaction validation status is VALID, the validator will validate the sender, and
will signal the receiver, that it has completed its validation.
Then it waits to receive the transaction status from the receiver.
- When the validator receives the transaction status as accepted and processed from
the receiver, it adds the transaction details to the local ledger and changes the balance
of the transaction node.
- Resets the used variables to participate in the next transaction.

###########ALGORITHM###########
1) Using the writerLock, generate a positive random amount for each node, and add each node
to the global ledger _blockChainData if it is not already there. Release the writerLock.
2) Using the readerLock, update each node's local ledger with the global data after adding each
node to the global ledger. Release the readerLock.
3) Using the writerLock, either take on the role of a sender, receiver, or validator. Release the
writerLock.
4) If sender,
- Display participating nodes and their initial amount from its local ledger.
- Acquire the transactionLock.
- Generate a transaction id, and random loan amount.
- Deduct loan amount from the sender's amount in his local ledger
- Inform the receiver about this new transaction.
pthread_cond_broadcast(&newTransaction);
- The sender waits for a transaction status from the receiver.
pthread_cond_wait(&transactionStatus, &transactionLock);
- If _transactionStatus is DECLINED, then rollback the transaction. Revert the changes
performed above in the sender's local ledger
- If _transactionStatus is ACCEPTED, then update the local ledger by adding the
transaction details and updating the node balances.- Release the transactionLock.
5) If receiver,
- Acquire the transactionLock.
- The receiver waits for a signal from the sender that a new transaction has started.
pthread_cond_wait(&newTransaction, &transactionLock);
- Once it gets the signal from the sender, it validates the sender by using the
localBlockChainData.
- If the sender is an invalid node,
- the receiver informs the waiting validators that the transaction is invalid.
_receiverTransactionValidity = INVALID;
pthread_cond_broadcast(&transactionValidationStatusByReceiver);
- The receiver informs the waiting sender that the transaction is declined.
_transactionStatus = DECLINED;
pthread_cond_broadcast(&transactionStatus);
- The receiver resets the global and local data.
- Release the transactionLock.
- If the sender is a valid node,
- Inform the waiting validators that the sender is valid. So that the validators can
validate the transaction.
_receiverTransactionValidity = VALID;
pthread_cond_broadcast(&transactionValidationStatusByReceiver);
- As the sender is valid, the receiver waits to get transaction validation status
response from all the validators.
pthread_cond_wait(&validatedTransaction, &transactionLock);
- Once the transaction is validated by the validators, the receiver updates the transaction
details in the local ledger localBlockChainData.
- Then the receiver updates the transaction details in the global ledger _blockChainData.
- The sender informs the waiting sender, and the waiting validators that the transaction is
valid, and hence it is accepted and processed by the receiver.
_transactionStatus = ACCEPTED;
pthread_cond_broadcast(&transactionStatus);
- The receiver resets the global and local data.
- Release the transactionLock.
6) If validator,
- Acquire the transactionLock.
- It will just wait to get the transaction validation status from the receiver
pthread_cond_wait(&transactionValidationStatusByReceiver, &transactionLock);.
- It will just wait to get the transaction validation status from the receiver.
- If the transaction validation status is VALID,
- the validator will validate the sender, and will signal the receiver that it has
completed its validation.
pthread_cond_signal(&validatedTransaction);
- Then it will wait for the transaction status from the receiver.
pthread_cond_wait(&transactionStatus, &transactionLock);- If the transaction status is accepted and processed, then it updates the local
ledger with the transaction details.
- Release the transactionLock.
- Reset the data.