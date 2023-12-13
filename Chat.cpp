#include <QMessageBox>
#include "Chat.h"

#include <iostream>
#include <QDockWidget>

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

ChatWindow::ChatWindow (const QString & host, quint16 port, QWidget * parent) :
  QMainWindow (parent),
  chat (host, port, this),
  text (this),
  input (this),
  participants (this)
{
  text.setReadOnly (true);
  setCentralWidget (&text);

  this->participants.setEditTriggers(QAbstractItemView::NoEditTriggers);


  // Insertion de la zone de saisie.
  // QDockWidget insérable en haut ou en bas, inséré en bas .
  // TODO

  // Désactivation de la zone de saisie.

  this->input.setEnabled (false);
  this->participants.setEnabled(false);



  // Envoi de messages lorsque la touche "entrée" est pressée.
  // - transmission du texte au moteur de messagerie instantanée ;
  // - effacement de la zone de saisie.
  // TODO

  // Connexion.
  // - affichage d'un message confirmant la connexion ;
  // - saisie de l'alias ;
  // - envoi de l'alias ;
  // - activation de la zone de saisie.
  // TODO

  // Déconnexion.
  // - désactivation de la zone de saisie.
  // - affichage d'un message pour signaler la déconnexion.
  // TODO

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
  // Connexion d'un utilisateur.
  // Déconnexion d'un utilisateur.
  // Nouvel alias d'un utilisateur.
  // Message privé.
  // TODO

  // Gestion des erreurs.
  connect (&chat, &Chat::error, [this] (const QString & id) {
    QMessageBox::critical (this, tr("Error"), id);
  });

  // CONNEXION !
  text.append (tr("<b>Connecting...</b>"));
}

