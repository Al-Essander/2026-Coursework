#include <iostream>
using namespace std;







struct Fan
{
    unsigned int id;

    double flowRate;  //Q^3
    double pressure; //Pressure
    double power; // Ny


    bool isValid;
};











unsigned short int menu()
{
    cout<<"fansearcher ver.0.1"<<endl;
    cout<<"1.test"<<endl;
    cout<<"0.exit"<<endl;

    cout<<"choice: ";


    unsigned short int choice;
    cin>>choice;

    return choice;
}




int main()
{
    while (true) {
        unsigned short int choice = menu();
        switch(choice)
        {
            case 0:
                break;



            case 1:
                cout<< "test case 1"<<endl;
                break;



            default:
                cout<<"unknown command..."<<endl;
                break;
        }
    }
}
