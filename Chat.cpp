#include <QMessageBox>
#include "Chat.h"
#include <map>
#include <iostream>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QInputDialog>

using namespace std;


////////////////////////////////////////////////////////////////////////////////
// Chat ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Processeurs.
const std::map<QString, Chat::Processor> Chat::PROCESSORS {
  {"#error", &Chat::process_error},
  {"#alias", &Chat::process_alias},
  {"#connected", &Chat::process_connected},
  {"#disconnected", &Chat::process_disconnected},
  {"#rename", &Chat::process_renamed},
  {"#renamed", &Chat::process_renamed},
  {"#list", &Chat::process_list},
  {"#private", &Chat::process_private}
};

// Constructeur.
Chat::Chat (const QString & host, quint16 port, QObject * parent) :
  QObject (parent),
  socket ()
{
  // Signal "connected" émis lorsque la connexion est effectuée.
    connect(&socket, &QTcpSocket::connected, [this, host, port](){
        emit connected (host, port);
    });


  // Signal "disconnected" émis lors d'une déconnexion du socket.
   connect(&socket, &QTcpSocket::disconnected, this, &Chat::disconnected);


  // Lecture.
  connect (&socket, &QIODevice::readyRead, [this] () {
    // Tant que l'on peut lire une ligne...
    while (socket.canReadLine ())
    {
      // Lecture d'une ligne et suppression du "newline".
      QString m = socket.readLine ().chopped (1);

      // Flot de lecture.
      QTextStream stream (&m);
      // Lecture d'une commande potentielle.
      QString command;
      stream >> command;

      // Recherche de la commande serveur dans le tableau associatif.
      // - si elle existe, traitement du reste du message par le processeur ;
      // - sinon, émission du signal "message" contenant la ligne entière.
      int firstSpaceIndex = command.indexOf(' ');
      QString mot = QStringRef(&m, 0, firstSpaceIndex).toString();
      bool trouve = PROCESSORS.count(mot) > 0;
      if(trouve)
      {
        QString message = QStringRef(&m, firstSpaceIndex, command.length() - firstSpaceIndex).toString();
        Chat::Processor proc = PROCESSORS.at(mot);
        (this->*proc)(stream);
      }
      else
      {
        this->write(command);
      }

    }
  });

  // CONNEXION !
  socket.connectToHost (host, port, QIODevice::ReadWrite, QAbstractSocket::IPv4Protocol);
}

Chat::~Chat ()
{
  // Déconnexion des signaux .
  socket.disconnect ();
}

// Commande "#alias"
void Chat::process_alias (QTextStream & is)
{
    QString id;
    is >> id;
    emit usr_alias(id);
}

// Commande "#connected"
void Chat::process_connected (QTextStream & is)
{
    QString id;
    is >> id >> ws;
    emit usr_connected(id);
}


// Commande "#disconnected"
void Chat::process_disconnected (QTextStream & is)
{
    QString id;
    is >> id >> ws;
    emit usr_disconnected(id);
}


// Commande "#renamed"
void Chat::process_renamed (QTextStream & is)
{
    QString oldUsername, newUsername;
    is >> oldUsername >> ws >> newUsername;
    emit usr_rename(oldUsername, newUsername);
}


// Commande "#list"
void Chat::process_list (QTextStream & is)
{
    is >> ws;
    QString line = is.readAll();
    QStringList list = line.split(' ');
    list.sort();
    emit usr_list(list);
}


// Commande "#private"
void Chat::process_private (QTextStream & is)
{
    QString id, message;
    is >> ws >> id >> ws;
    emit private_msg (id, is.readAll());
}


// Commande "#error"
void Chat::process_error (QTextStream & is)
{
  QString id;
  is >> id >> ws;
  emit error (id);
}


// Envoi d'un message à travers le socket.
void Chat::write (const QString & message)
{
  socket.write (message.toUtf8 () + '\n');
}

////////////////////////////////////////////////////////////////////////////////
// ChatWindow //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



PrivateChat* ChatWindow::createOrShowWindow(const QString& recipent)
{
    if(alias == recipent)
        return nullptr;

    auto search = privateChats.find(recipent);
    if(search != privateChats.end())
    {
        if(!search.value()->isVisible())
            tabWidget.addTab(search.value(), search.key());

        int index = tabWidget.indexOf(search.value());
        tabWidget.setCurrentIndex(index);
        tabWidget.setTabEnabled(index, true);
        tabWidget.show();
        return search.value();
    }
    else
    {
        PrivateChat* privChat = new PrivateChat() ;
        this->privateChats.insert(recipent, privChat);
        tabWidget.addTab(privChat, recipent);

        connect(&privChat->lineEdit, &QLineEdit::returnPressed, [this, privChat, recipent] ()
        {
            QString message = privChat->lineEdit.text();
            privChat->message(message);
            this->chat.write("/private " + recipent + " " + message);
            privChat->lineEdit.clear();
        });

        tabWidget.show();
        return privChat;
    }
}


ChatWindow::ChatWindow (const QString & host, quint16 port, QWidget * parent) :
  QMainWindow (parent),
  chat (host, port, this),
  text (this),
  input (this),
  participants (this)
{
  text.setReadOnly (true);
  setCentralWidget (&text);


  connect(&tabWidget, &QTabWidget::tabCloseRequested, [this] (int index) {
      tabWidget.removeTab(index);
  });


  this->participants.setEditTriggers(QAbstractItemView::NoEditTriggers);


  // Insertion de la zone de saisie.
  // QDockWidget insérable en haut ou en bas, inséré en bas .
  QDockWidget* dockInputWidget = new QDockWidget("Zone de saisie", nullptr, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
  dockInputWidget->setWidget(&input);
  dockInputWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
  this->addDockWidget(Qt::BottomDockWidgetArea, dockInputWidget);


  // Désactivation de la zone de saisie.

  this->input.setEnabled (false);
  this->participants.setEnabled(false);

  connect(&participants, &QListView::doubleClicked, [this] (const QModelIndex& index) {
      QString recipent = this->model_participants.data(index).toString();
      createOrShowWindow(recipent);
});


  // Envoi de messages lorsque la touche "entrée" est pressée.
  // - transmission du texte au moteur de messagerie instantanée ;
  // - effacement de la zone de saisie.
  connect(&input, &QLineEdit::returnPressed, [this] () {
       this->chat.write(input.text());
       this->input.clear();
   });


  // Connexion.
  // - affichage d'un message confirmant la connexion ;
  // - saisie de l'alias ;
  // - envoi de l'alias ;
  // - activation de la zone de saisie.
  connect(&chat, &Chat::connected, [this] () {
      this->text.append("Connecté !");
      this->text.append("Tapez votre Pseudonyme");
      bool ok;
      QString alias = QInputDialog::getText(this, "Connection", "Pseudonyme : ", QLineEdit::Normal, "", &ok);

      if(ok && !alias.isEmpty() && !alias.contains(" "))
      {
          this->chat.write(alias);
          input.setEnabled(true);
          this->participants.setEnabled(true);
      }
  });


  // Déconnexion.
  // - désactivation de la zone de saisie.


  // - affichage d'un message pour signaler la déconnexion.
  connect(&chat, &Chat::disconnected, [this] () {
      this->text.append("Disconnected!");
      this->input.setEnabled(false);
      this->participants.setEnabled(false);
      this->model_participants.setStringList(QStringList());
      this->model_participants.dataChanged(model_participants.index(0),
                                     model_participants.index(model_participants.rowCount()));
  });


  // Messages.
  connect (&chat, &Chat::message, [this] (const QString & message) {
    text.append (message);
  });

  // Liste des utilisateurs.
  QDockWidget* dockParticpantsWidget = new QDockWidget("Liste des utilisateurs", nullptr, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
  dockParticpantsWidget->setWidget(&participants);
  dockParticpantsWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
  this->addDockWidget(Qt::RightDockWidgetArea, dockParticpantsWidget);
  this->participants.setModel(&model_participants);

  connect(&participants, &QListView::doubleClicked, [this] (const QModelIndex& index) {
      QString recipent = this->model_participants.data(index).toString();
      auto search = privateChats.find(recipent);
      if(search != privateChats.end())
      {
          tabWidget.setCurrentWidget(search.value());
      }
      else
      {
          PrivateChat* privChat = new PrivateChat() ;
          this->privateChats.insert(recipent, privChat);
          tabWidget.addTab(privChat, recipent);

          connect(&privChat->lineEdit, &QLineEdit::returnPressed, [this, privChat, recipent] ()
          {
              QString message = privChat->lineEdit.text();
              privChat->message(message);
              this->chat.write("/private " + recipent + " " + message);
              privChat->lineEdit.clear();
          });
      }
      tabWidget.show();
      });



  // Connexion d'un utilisateur.
  // Déconnexion d'un utilisateur.
  // Nouvel alias d'un utilisateur.
  // Message privé.
  connect(&chat, &Chat::usr_connected, [this] (const QString & username) {
     this->text.append("#connected " + username);
     QStringList list = model_participants.stringList ();
     list.append(username);
     list.sort();
     this->model_participants.setStringList (list);
     this->model_participants.dataChanged(model_participants.index(0),
                                    model_participants.index(model_participants.rowCount()));
  });

  connect(&chat, &Chat::usr_disconnected, [this] (const QString & username) {
     this->text.append("#disconnected " + username);
     QStringList list = model_participants.stringList ();
     list.removeOne(username);
     this->model_participants.setStringList (list);
     this->model_participants.dataChanged(model_participants.index(0), model_participants.index(model_participants.rowCount()));
  });

  connect(&chat, &Chat::usr_list, [this] (const QStringList & users) {
     this->text.append("#list " + users.join(" "));
     this->model_participants.setStringList (users);
     this->model_participants.dataChanged(model_participants.index(0), model_participants.index(model_participants.rowCount()));
  });

  connect(&chat, &Chat::usr_rename, [this] (const QString & oldUsername, const QString & newUsername) {
     this->text.append("#rename " + oldUsername + " " + newUsername);
      QStringList list = this->model_participants.stringList();
      list.replace(list.indexOf(oldUsername), newUsername);
      this->model_participants.setStringList (list);
      this->model_participants.dataChanged(model_participants.index(0), model_participants.index(model_participants.rowCount()));
  });

  connect(&chat, &Chat::private_msg, [this] (const QString & sender, const QString & message) {
     this->text.append("#private " + sender + " " + message);

  });

  connect(&chat, &Chat::usr_alias, [this] (const QString & username) {
     this->text.append("#alias " + username);
  });


  // Gestion des erreurs.
  connect (&chat, &Chat::error, [this] (const QString & id) {
    QMessageBox::critical (this, tr("Error"), id);
  });

  // CONNEXION !
  text.append (tr("<b>Connexion...</b>"));

}

////////////////////////////////////////////////////////////////////////////////
// PrivateChat //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

PrivateChat::PrivateChat(QWidget *parent) : QWidget(parent), textEdit(), lineEdit()
{
    QVBoxLayout* layout = new QVBoxLayout;

    layout->addWidget(&textEdit, true);
    textEdit.setReadOnly (true);

    layout->addWidget(&lineEdit, true);
    setLayout(layout);
}

void PrivateChat::message(const QString& message)
{
    textEdit.append(message);
}
