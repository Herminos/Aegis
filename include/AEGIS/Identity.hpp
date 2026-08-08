#include<string>
#include<unordered_map>

struct PeerIdentity{
    std::string name="";
};

class Identity{
    public:
        Identity(const std::string& id_file_path);
    private:
        std::unordered_map<std::string, PeerIdentity> peer_identities;

};