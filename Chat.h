#ifndef CHAT_H
#define CHAT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QListView>
#include <QStringListModel>
#include <QMimeData>
#include <QFileInfo>
#include <QImageReader>

// Chat hérite de QObject
class Chat : public QObject
{
  Q_OBJECT // signaux + slots

  private:
    // Signature d'une méthode dédiée au traitement d'un type de message.
    typedef void (Chat::*Processor) (QTextStream &);
    // Tableau associatif statique qui associe une chaîne de caractères à un processeur.
    static const std::map<QString, Processor> PROCESSORS;

  private:
    QTcpSocket socket;

  private:
    // Traitement d'un message.
    void process (const QString & message);
    // Gestion des erreurs.
    void process_error (QTextStream &);
    void process_alias (QTextStream &);
    void process_connected (QTextStream &);
    void process_disconnected (QTextStream &);
    void process_renamed (QTextStream &);
    void process_list (QTextStream &);
    void process_private (QTextStream &);

  public:
    // constructeur : nom du serveur, port et, éventuellement, objet parent.
    Chat (const QString & host, quint16 port, QObject * parent = nullptr);
    ~Chat ();

    // Envoi d'un message.
    void write (const QString &);

  signals:
    // Connexion / déconnexion.
    void connected (const QString & host, quint16 port);
    void disconnected ();
    // Message.
    void message (const QString & message);
    // Error.
    void error (const QString & id);
    void usr_alias (const QString & id);
    void usr_list (const QStringList & users);
    void usr_connected (const QString & id);
    void usr_disconnected (const QString & id);
    void usr_rename (const QString & oldUsername, const QString & newUsername);
    void private_msg(const QString & sender, const QString & message);
    void receiveMessage (const QString & message);
};

class PrivateChat : public QWidget
{
    Q_OBJECT

public:
    QTextEdit textEdit;
    QLineEdit lineEdit;

    explicit PrivateChat(QWidget *parent = nullptr);
    void message(const QString& message);
};

// ChatWindow hérite de QMainWindow.
class ChatWindow : public QMainWindow
{
  Q_OBJECT

  private:
    // Moteur de messagerie instantanée.
    Chat chat;

    QString alias;

    QTextEdit text;
    // Zone de saisie.
    QLineEdit input;

    // Zone des participants
    QListView participants;
    // Model de la liste des participants

    QStringListModel model_participants;

    QTabWidget tabWidget;

    QMap<QString, PrivateChat*> privateChats;

    PrivateChat* createOrShowWindow(const QString& recipent);

  public:
    // Constructeur.
    ChatWindow (const QString & host, quint16 port, QWidget * parent = nullptr);
};




#endif // CHAT_H

