﻿
#include <MyAccessHub.h>
#include "LightSettingManager.h"


//平行光源データのコンストラクタ
DirectionalLightContainer::DirectionalLightContainer()
{
    auto myEngine = MyAccessHub::getMyGameEngine();

    m_lightData.Color = XMVectorSet(0, 0, 0, 0);        //初期は全て0
    m_lightData.Direction = XMVectorSet(0, 0, 0, 0);

    myEngine->CreateConstantBuffer(m_cBuff.GetAddressOf(), &m_lightData,
        sizeof(DirectionalLightContainer::DirectionalLightData));       //シェーダリソース作成

    //======Depth Shadow
    m_lightViewMtx = {};
    myEngine->CreateConstantBuffer(m_lightMtxBuff.GetAddressOf(), &m_lightViewMtx, sizeof(XMMATRIX));
    //======Depth Shadow End
}

void DirectionalLightContainer::SetDirectionalLight(XMFLOAT3 color, XMFLOAT3 direction)
{
    m_lightData.Color = XMVectorSet(color.x, color.y, color.z, 0.0f);   //引数の値を反映

    //ConstantBufferはXMFLOAT3だとシェーダ側でずれる。XMVECTORに変換
    m_lightData.Direction = XMVectorSet(direction.x, direction.y, direction.z, 0.0f);

    UpdateCBuffer();
}

DirectionalLightContainer::DirectionalLightData& DirectionalLightContainer::GetLightData()
{
    return m_lightData;     //光源データ取得
}

ID3D12Resource* DirectionalLightContainer::GetConstantBuffer()
{
    return m_cBuff.Get();   //シェーダリソース取得
}

//======Depth Shadow
void DirectionalLightContainer::UpdateLightBaseMatrix(XMFLOAT3& eyePos, XMFLOAT3& focusPos)
{
    XMVECTOR camPosVect = XMLoadFloat3(&eyePos);        //カメラ位置
    XMVECTOR focusPosVect = XMLoadFloat3(&focusPos);    //フォーカスする(カメラが向く)位置
    XMVECTOR lightDir = m_lightData.Direction;

    //フォーカス位置から光の方向の逆に現在のカメラ距離の20倍移動した位置を光源位置としている
    XMVECTOR lightPos = focusPosVect - XMVector3Normalize(lightDir) * XMVector3Length(XMVectorSubtract(focusPosVect, camPosVect)).m128_f32[0] * 20.0f;

    //CameraComponentと同じ処理
    XMVECTOR Up = XMVectorSet(0, 1, 0, 0.0f);   //カメラの上方向単位ベクトル（カメラのロール軸）
    m_lightViewMtx = XMMatrixLookAtLH(lightPos, focusPosVect, Up);  //View行列作成
    XMMATRIX trMat = XMMatrixTranspose(m_lightViewMtx);

    auto myEngine = MyAccessHub::getMyGameEngine();
    myEngine->UpdateShaderResourceOnGPU(m_lightMtxBuff.Get(), &trMat, sizeof(XMMATRIX));    //シェーダリソース更新
}

XMMATRIX DirectionalLightContainer::GetLightViewMtx()
{
    return m_lightViewMtx;
}

ID3D12Resource* DirectionalLightContainer::GetLightBaseMtxBuffer()
{
    return m_lightMtxBuff.Get();
}
//======Depth Shadow End

void DirectionalLightContainer::UpdateCBuffer()
{
    auto myEngine = MyAccessHub::getMyGameEngine();

    myEngine->UpdateShaderResourceOnGPU(m_cBuff.Get(), &m_lightData,
        sizeof(DirectionalLightContainer::DirectionalLightData));  //更新処理
}

//===DirectionalLightContainer End===



AmbientLightContainer::AmbientLightContainer()
{
    auto myEngine = MyAccessHub::getMyGameEngine();
    m_color = XMVectorZero();   //初期値は0
    
    //シェーダリソース作成
    myEngine->CreateConstantBuffer(m_cBuff.GetAddressOf(), &m_color, sizeof(XMVECTOR));
}

void AmbientLightContainer::UpdateCBuffer()
{
    auto myEngine = MyAccessHub::getMyGameEngine();
    myEngine->UpdateShaderResourceOnGPU(m_cBuff.Get(), &m_color, sizeof(XMVECTOR)); //更新処理
}

void AmbientLightContainer::SetLight(float r, float g, float b)
{
    m_color = XMVectorSet(r, g, b, 1.0f);   //設定+更新処理
    UpdateCBuffer();
}

void AmbientLightContainer::SetLight(XMFLOAT3 color)
{
    SetLight(color.x, color.y, color.z);    //float引数のメソッドを呼んでいるだけ
}

//===AmbientLightContainer End===



LightSettingManager::LightSettingManager()
{
    m_AmbientLights.clear();
    m_DirectionalLights.clear();
}

LightSettingManager* LightSettingManager::GetInstance()
{
    static LightSettingManager* instance = new LightSettingManager();
    return instance;
}

void LightSettingManager::CreateAmbientLight(std::wstring label, XMFLOAT3 light)
{
    if (m_AmbientLights.find(label) != m_AmbientLights.end())
    {
        m_AmbientLights[label]->SetLight(light);    //同じ名前のデータがある場合はデータの書き換えだけで終わる
        return;
    }

    std::unique_ptr<AmbientLightContainer> ambLight = std::make_unique<AmbientLightContainer>();
    ambLight->SetLight(light);

    m_AmbientLights[label] = std::move(ambLight);
}

void LightSettingManager::CreateDirectionalLight(std::wstring label, XMFLOAT3 light, XMFLOAT3 direction)
{
    if (m_DirectionalLights.find(label) != m_DirectionalLights.end())
    {
        m_DirectionalLights[label]->SetDirectionalLight(light, direction);  //同じ名前のデータがあるので書き換えのみ
        return;
    }

    std::unique_ptr<DirectionalLightContainer> dirLight = std::make_unique<DirectionalLightContainer>();
    dirLight->SetDirectionalLight(light, direction);

    m_DirectionalLights[label] = std::move(dirLight);
}

AmbientLightContainer* LightSettingManager::GetAmbientLight(std::wstring label)
{
    if (m_AmbientLights.find(label) == m_AmbientLights.end())
        return nullptr; //名前のデータがない場合はnullptrを返す

    return m_AmbientLights[label].get();
}

DirectionalLightContainer* LightSettingManager::GetDirectionalLight(std::wstring label)
{
    if (m_DirectionalLights.find(label) == m_DirectionalLights.end())
        return nullptr; //名前のデータがない場合はnullptrを返す

    return m_DirectionalLights[label].get();
}


void LightSettingManager::DeleteAmbientLight(std::wstring label)
{
    if (m_AmbientLights.find(label) != m_AmbientLights.end())
        m_AmbientLights.erase(label);       //該当データがあれば削除
}

void LightSettingManager::DeleteDirectionalLight(std::wstring label)
{
    if (m_DirectionalLights.find(label) != m_DirectionalLights.end())
        m_DirectionalLights.erase(label);   //該当データがあれば削除
}

LightSettingManager::~LightSettingManager()
{
    m_AmbientLights.clear();        //uniqur_ptrなのでclearされたら全てデストラクタが動く
    m_DirectionalLights.clear();
}

//===LightSettingManager End===
