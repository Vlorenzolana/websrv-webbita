#include "../includes/User.hpp"

User::User(unsigned int user_id, std::string user_name, std::string user_pasword) :
_user_id(user_id), _user_name(user_name), _user_pasword(user_pasword), _times_conected(1), _user_email()
{}

User::User(unsigned int user_id, std::string user_name, std::string user_pasword, std::string user_email) :
_user_id(user_id), _user_name(user_name), _user_pasword(user_pasword), _times_conected(1), _user_email(user_email)
{}

User::~User(){}

unsigned int User::getId() const {return (_user_id);};
unsigned int User::getTimesConected() const {return (_times_conected);};
std::string User::getName() const {return (_user_name);};
std::string User::getPasword() const {return (_user_pasword);};
std::string User::getEmail() const {return (_user_email);}

void User::incriseTimesConected() {_times_conected = _times_conected + 1;}

