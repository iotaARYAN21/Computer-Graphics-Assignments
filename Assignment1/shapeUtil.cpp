#include<bits/stdc++.h>
using namespace std;
void generateGrid(vector<float>& a,int n){
    for(int y=0;y<=n;y++){
        for(int z=0;z<=n;z++){
            a.push_back(0);
            a.push_back(y);
            a.push_back(z);

            a.push_back(n);
            a.push_back(y);
            a.push_back(z);
        }
    }

    //y 
    for(int x=0;x<=n;x++){
        for(int z=0;z<=n;z++){
            a.push_back(x);
            a.push_back(0);
            a.push_back(z);

            a.push_back(x);
            a.push_back(n);
            a.push_back(z);
        }
    }

    //z
    for(int x=0;x<=n;x++){
        for(int y=0;y<=n;y++){
            a.push_back(x);
            a.push_back(y);
            a.push_back(0);

            a.push_back(x);
            a.push_back(y);
            a.push_back(n);
        }
    }
}
// void generateCube(vector<float>& a, int x, int y, int z)
// {
//     // 8 corners of the cube
//     a = {
//         (float)x,     (float)y,     (float)z,       // 0
//         (float)x + 1, (float)y,     (float)z,       // 1
//         (float)x + 1, (float)y + 1, (float)z,       // 2
//         (float)x,     (float)y + 1, (float)z,       // 3

//         (float)x,     (float)y,     (float)z + 1,   // 4
//         (float)x + 1, (float)y,     (float)z + 1,   // 5
//         (float)x + 1, (float)y + 1, (float)z + 1,   // 6
//         (float)x,     (float)y + 1, (float)z + 1    // 7
//     };
// }

void generateCube(vector<float>& a){
    a={
         0,0,0,
        1,0,0,
        1,1,0,
        0,1,0,

        0,0,1,
        1,0,1,
        1,1,1,
        0,1,1
    };
}