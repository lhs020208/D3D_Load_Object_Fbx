#include <iostream>
#include <string>
#include <fstream>
using namespace std;

// FBX SDK include
#include <fbxsdk.h>

int main()
{
    cout << "FBX 파일 이름을 입력하십시오.(ex: model): ";
    string s;
    cin >> s;

    string fbxFileName = s + ".fbx";
    string binFileName = s + ".bin";

    // 1) FBX SDK 초기화
    FbxManager* manager = FbxManager::Create();
    if (!manager)
    {
        cout << "FBX Manager 생성 실패.\n";
        return -1;
    }

    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    // 2) FBX 파일 읽기 시도
    FbxImporter* importer = FbxImporter::Create(manager, "");
    bool ok = importer->Initialize(fbxFileName.c_str(), -1, manager->GetIOSettings());

    if (!ok)
    {
        cout << "FBX 파일을 열 수 없습니다: " << fbxFileName << "\n";
        importer->Destroy();
        manager->Destroy();
        return -1;
    }

    // 읽기 성공 메시지
    cout << "FBX 파일 열기 성공: " << fbxFileName << "\n";

    // Import를 수행해야 importer가 끝까지 정상적으로 돌아간다
    FbxScene* scene = FbxScene::Create(manager, "scene");
    importer->Import(scene);

    importer->Destroy();

    // 3) BIN 파일 생성 (내용: 'a' 1바이트)
    ofstream fout(binFileName, ios::binary);
    if (!fout)
    {
        cout << "BIN 파일 생성 실패: " << binFileName << "\n";
        manager->Destroy();
        return -1;
    }

    char c = 'a';
    fout.write(&c, 1);
    fout.close();

    cout << "BIN 파일 생성 완료: " << binFileName << "\n";

    // 정리
    manager->Destroy();

    return 0;
}
