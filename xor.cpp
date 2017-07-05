#include <fstream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <iostream>

using namespace std;


int main(int argc, char** argv)
{
	vector<vector<char> > data;
	vector<char> d;

	for(int i=1;i<=6;i++)
	{
		char buffer[65536];
//		cout << "reading " << argv[i] << endl;
		ifstream myFile (argv[i], ios::in | ios::binary);
		myFile.read (buffer, 65536);
		myFile.close();
		d.clear();
		for(int i=0;i<65536;i++)
		{
			d.push_back(buffer[i]);
		}
		data.push_back(d);
	}


	vector<char> r;
	r=data[0];
	for(int i=1;i<data.size()-1;i++)
	{
//		cout << "xoring " << i << endl;
		for(int j=0;j<65536;j++)
		{
			r[j]=(r[j]^data[i][j]);
		}
	}
	int s=0;
//	cout << "comparing with " << data.size()-1 << endl;
	for(int j=0;j<65536;j++)
	{
//		cout << j << " " << (int)r[j] << " " << (int)data[data.size()-1][j] <<endl;
		s=s+abs(r[j]-data[data.size()-1][j]);
	}
	cout << s << endl;




	return 0;	
}

