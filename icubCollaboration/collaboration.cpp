#include "collaboration.h"

bool    collaboration::configure(ResourceFinder &rf)
{
    name=rf.check("name",Value("icubCollaboration")).asString().c_str();
    robot=rf.check("robot",Value("icub")).asString().c_str();
    part=rf.check("part",Value("right_arm")).asString().c_str();
    posTol=rf.check("period",Value(0.001)).asDouble();
    if (part == "right_arm")
        _arm = "right";
    else if (part == "left_arm")
        _arm = "left";
    period=rf.check("period",Value(0.0)).asDouble();    // as default, update module as soon as receiving new parts from skeleton2D

    // Workspace
    workspaceX=rf.check("workspaceX",Value(-0.5)).asDouble();
    workspaceY=rf.check("workspaceY",Value(0.3)).asDouble();
    workspaceZ_low=rf.check("workspaceZ_low",Value(-0.1)).asDouble();
    workspaceZ_high=rf.check("workspaceZ_high",Value(0.35)).asDouble();

    // OPC client
    partner_default_name=rf.check("partner_default_name",Value("partner")).asString().c_str();
    string opcName=rf.check("opc",Value("OPC")).asString().c_str();
    opc = new OPCClient(name);

    while (!opc->connect(opcName))
    {
        yInfo()<<"Waiting connection to OPC...";
        Time::delay(1.0);
    }
    opc->checkout();

    // Port to react-Control
    rpcReactCtrl.open(("/"+name+"/reactController/rpc:o").c_str());
    std::string reactCtrlRPC = "/reactController/rpc:i";
    connectedReactCtrl = yarp::os::Network::connect(rpcReactCtrl.getName().c_str(), reactCtrlRPC);
    while (!connectedReactCtrl)
    {
        connectedReactCtrl = yarp::os::Network::connect(rpcReactCtrl.getName().c_str(), reactCtrlRPC);
        yInfo()<<"Waiting connection to reactController...";
        Time::delay(1.0);
    }

    // ARE
    rpcARE.open(("/"+name+"/ARE/cmd:io").c_str());
    std::string actionsRenderingEngineRPC = "/actionsRenderingEngine/cmd:io";
    connectedARE = yarp::os::Network::connect(rpcARE.getName().c_str(), actionsRenderingEngineRPC);

    // TODO: Cartesian Controller

    yarp::os::Property OptT;
    OptT.put("robot",  robot);
    OptT.put("part",   "torso");
    OptT.put("device", "remote_controlboard");
    OptT.put("remote", "/"+robot+"/torso");
    OptT.put("local",  "/"+name +"/torso");
    if (!ddT.open(OptT))
    {
        yError("[reactCtrlThread]Could not open torso PolyDriver!");
        return false;
    }

    bool okT = 1;

    if (ddT.isValid())
    {
        okT = okT && ddT.view(iencsT);
        okT = okT && ddT.view(ivelT);
        okT = okT && ddT.view(iposT);
        okT = okT && ddT.view(imodT);
        okT = okT && ddT.view(ilimT);

    }
    iencsT->getAxes(&jntsT);
    encsT = new yarp::sig::Vector(jntsT,0.0);

    if (!okT)
    {
        yError("[reactCtrlThread]Problems acquiring torso interfaces!!!!");
        return false;
    }

    // Arm Cartesian Controller: for current pose checking
    Property optArm("(device cartesiancontrollerclient)");
    optArm.put("remote",("/"+robot+"/cartesianController/" + part).c_str());
    optArm.put("local",("/"+name+"/cart_ctrl/"+ part).c_str());

    if ((!ddA.open(optArm)) || (!ddA.view(icartA)))
    {
        yError(" could not open the Arm Controller!");
        return false;
    }
    icartA -> storeContext(&contextArm);

    // Gaze controller
    Property OptGaze;
    OptGaze.put("device","gazecontrollerclient");
    OptGaze.put("remote","/iKinGazeCtrl");
    OptGaze.put("local","/"+name+"/gaze");

    if ((!ddG.open(OptGaze)) || (!ddG.view(igaze)))
    {
        yError(" could not open the Gaze Controller!");
        return false;
    }

    igaze -> storeContext(&contextGaze);
    igaze -> setSaccadesMode(false);
    igaze -> setNeckTrajTime(0.75);
    igaze -> setEyesTrajTime(0.5);

    homeAng.resize(3,0.0);
    homeAng[0]=+0.0;   // azimuth-relative component wrt the current configuration [deg]
    homeAng[1]=-25.0;   // elevation-relative component wrt the current configuration [deg]
    homeAng[2]=+0.0;   // vergence-relative component wrt the current configuration [deg]

    igaze -> lookAtAbsAnglesSync(homeAng);
    igaze -> waitMotionDone(0.1,5.0);

    Vector curAng(3,0.0);
    igaze->getAngles(curAng);
    yDebug("current angle %s",curAng.toString(3,3).c_str());

    // rpc port
    rpcPort.open("/"+name+"/rpc");
    attach(rpcPort);

    // home position for reactCtrl
    homePosL.resize(3, 0.05);
    homePosL[0] = -0.2;
    homePosL[1] = -0.3;

    homePosR.resize(3, 0.05);
    homePosR[0] = -0.2;
    homePosR[1] = +0.3;

    basket.resize(3, 0.0);
    basket[0] = 0.2;
    basket[1] = 0.4;
    basket[2] = 0.05;
    // TODO set this

    return true;
}

bool    collaboration::interruptModule()
{
    yDebug("[%s] Interupt module",name.c_str());

    homeARE();
    yDebug("[%s] Remove partner", name.c_str());
    opc->checkout();
    opc->interrupt();
    rpcPort.interrupt();
    rpcARE.interrupt();
    rpcReactCtrl.interrupt();

    return true;
}

bool    collaboration::close()
{
    yDebug("[%s] closing module",name.c_str());
    rpcPort.close();
    opc->close();
    rpcARE.close();
    rpcReactCtrl.close();

    yInfo("Closing gaze controller..");
    Vector ang(3,0.0);
    igaze -> lookAtAbsAngles(ang);
    igaze -> restoreContext(contextGaze);
    igaze -> stopControl();
    ddG.close();

    delete encsT; encsT = NULL;
    ddT.close();

    return true;
}

bool    collaboration::attach(RpcServer &source)
{
    return this->yarp().attachAsServer(source);
//    return true;
}

double  collaboration::getPeriod()
{
    return period;
}

bool    collaboration::updateModule()
{
    return true;
}

bool    collaboration::moveReactPPS(const string &target, const string &arm, const double &timeout)
{
    // get object pos with name <-- target
    Entity* e = opc->getEntity(target, true);
    Object *o;
    if(e) {
        o = dynamic_cast<Object*>(e);
    }
    else {
        yError() << target << " is not an Entity";
        return false;
    }
    if(!o) {
        yError() << "Could not cast" << e->name() << "to Object";
        return false;
    }

    o->m_value = -1.0;


    Vector offset(3,0.0);
    offset[0] += 0.05; // 5cm closer to robot
    if (arm=="right")
        offset[1] +=0.05;   //5cm on the right
    else if (arm=="left")
        offset[1] -=0.05;   //5cm on the left

    Vector targetPos = o->m_ego_position;
    opc->commit(o);
    if (checkPosReachable(targetPos+offset, arm))
        return moveReactPPS(targetPos+offset, arm, timeout);
    else
    {
        yDebug("%s is unreachable", (targetPos+offset).toString(3,3).c_str());
        return false;
    }
}

bool    collaboration::moveReactPPS(const Vector &pos, const string &arm, const double &timeout)
{
    // TODO: arm to decide which hand
    bool ok=false, done=false, completed=false;

    yDebug () << "ReactCtrl::set_xd start with " << arm.c_str();
    Bottle cmd, bPos, rep;
    cmd.addString("set_xd");
    for (int8_t i=0; i<pos.size(); i++)
        bPos.addDouble(pos[i]);
    cmd.addList() = bPos;

    yDebug("Command sent to ReactCtrl: %s",cmd.toString().c_str());
    if (rpcReactCtrl.write(cmd, rep))
        ok = rep.get(0).asBool();

    double checkTime, start=Time::now();
    while (ok && !done)
    {
        Vector x_cur(3,0.0), o_cur(4,0.0);
        icartA->getPose(x_cur, o_cur);
        Time::delay(period);
        checkTime = Time::now();
        completed = (norm(x_cur-pos)<=posTol);
        done =  completed || ((checkTime-start)>=timeout);
        if (done)
        {
            yDebug("xf= %s; x_cur= %s",pos.toString(3,3).c_str(),x_cur.toString(3,3).c_str());
            yDebug("checktime %f",checkTime-start);
        }
    }
    if (checkTime-start+1.0<=timeout)
        Time::delay(1.0);
    stop_React();

    return (ok);
}

bool    collaboration::homeARE()
{
    yDebug() << "ARE::home start";

    Bottle cmd, rep;
    bool ret = false;

    cmd.addVocab(Vocab::encode("home"));
    cmd.addString("all");

    yDebug("Command sent to ARE: %s",cmd.toString().c_str());

    if (rpcARE.write(cmd, rep))
        ret = (rep.get(0).asVocab()==Vocab::encode("ack"));

    yDebug() << "[homeARE] Reply from ARE: " << rep.toString();
    return ret;
}

bool    collaboration::stopReactPPS()
{
    yDebug () << "ReactCtrl::stop start";
    Bottle cmd, rep;
    cmd.addString("stop");

    yDebug("Command sent to ReactCtrl: %s",cmd.toString().c_str());
    if (rpcReactCtrl.write(cmd, rep))
        return rep.get(0).asBool();
    else
        return false;
}

bool    collaboration::takeARE(const string &target, const string &arm)
{
    // get object pos with name <-- target
    yDebug() << "ARE::take start";

    Entity* e = opc->getEntity(target, true);
    Object *o;
    if(e) {
        o = dynamic_cast<Object*>(e);
    }
    else {
        yError() << target << " is not an Entity";
        return false;
    }
    if(!o) {
        yError() << "Could not cast" << e->name() << "to Object";
        return false;
    }

    Vector targetPos = o->m_ego_position;
    // TODO: check if use takeARE or graspARE
    if (checkPosReachable(targetPos, arm))
//        return takeARE(targetPos, arm);
        return graspARE(targetPos, arm);
    else
    {
        yDebug("%s is unreachable", targetPos.toString(3,3).c_str());
        return false;
    }
}

bool    collaboration::takeARE(const Vector &pos, const string &arm)
{
    Bottle cmd, target, rep;
    bool ret = false;

    cmd.addVocab(Vocab::encode("take"));
    target.addString("cartesian");
    for (int8_t i=0; i<pos.size(); i++)
        target.addDouble(pos[i]);
    cmd.addList() = target;
    cmd.addString(arm.c_str());
    cmd.addString("side");
    cmd.addString("still");

    yDebug("Command sent to ARE: %s",cmd.toString().c_str());

    if (rpcARE.write(cmd, rep))
        ret = (rep.get(0).asVocab()==Vocab::encode("ack"));

    yDebug() << "[takeARE] Reply from ARE: " << rep.toString();
    return ret;
}

bool    collaboration::graspARE(const Vector &pos, const string &arm)
{
    Bottle cmd, target, rep;
    bool ret = false;

    cmd.addVocab(Vocab::encode("grasp"));
    target.addString("cartesian");
    for (int8_t i=0; i<pos.size(); i++)
        target.addDouble(pos[i]);

//    Vector rpy(3,0.0);
//    rpy[0] = 77.0*M_PI/180.0;
//    rpy[0] = -4.0*M_PI/180.0;
//    rpy[2] = 170.0*M_PI/180.0;
//    Vector rot = dcm2axis(rpy2dcm(rpy));
    Matrix R(3,3);
    // pose x-axis y-axis z-axis: palm inward, pointing forward
    R(0,0)=-1.0; R(0,1)= 0.0; R(0,2)= 0.0; // x-coordinate
    R(1,0)= 0.0; R(1,1)= 0.0; R(1,2)=-1.0; // y-coordinate
    R(2,0)= 0.0; R(2,1)=-1.0; R(2,2)= 0.0; // z-coordinate

    if (arm=="left")
        R(1,2) = 1.0;

    Vector rot = dcm2axis(R);
    yDebug("grasp rot=%s", rot.toString(3,3).c_str());
    for (int8_t i=0; i<rot.size(); i++)
        target.addDouble(rot[i]);

    cmd.addList() = target;
    cmd.addString(arm.c_str());
    cmd.addString("still");

    yDebug("Command sent to ARE: %s",cmd.toString().c_str());
    if (rpcARE.write(cmd, rep))
        ret = (rep.get(0).asVocab()==Vocab::encode("ack"));

    yDebug() << "[graspARE] Reply from ARE: " << rep.toString();
    return ret;

}

bool    collaboration::giveARE(const string &target, const string &arm)
{
    // target should be human empty hand :)
    yDebug() << "ARE::give start";

    Entity* e = opc->getEntity(partner_default_name, true);
    Agent* a;
    if(e) {
        a = dynamic_cast<Agent*>(e);
    }
    else {
        yError() << target << " is not an Entity";
        return false;
    }
    if(!a) {
        yError() << "Could not cast" << e->name() << "to Agent";
        return false;
    }
    Vector pos = a->m_body.m_parts[target.c_str()];

    Vector offset(3,0.0);
    offset[0] += 0.05; // 5cm closer to robot

    if (checkPosReachable(pos+offset, arm))
        return giveARE(pos+offset, arm);
    else
    {
        yDebug("%s is unreachable", (pos+offset).toString(3,3).c_str());
        return false;
    }
}

bool    collaboration::giveARE(const Vector &pos, const string &arm)
{
    Bottle cmd, target, rep;
    bool ret = false;

    cmd.addVocab(Vocab::encode("give"));
    target.addString("cartesian");
    for (int8_t i=0; i<pos.size(); i++)
        target.addDouble(pos[i]);
    cmd.addList() = target;
    cmd.addString(arm.c_str());
    cmd.addString("side");
    cmd.addString("still");

    yDebug("Command sent to ARE: %s",cmd.toString().c_str());

    if (rpcARE.write(cmd, rep))
        ret = (rep.get(0).asVocab()==Vocab::encode("ack"));

    yDebug() << "[takeARE] Reply from ARE: " << rep.toString();
    return ret;
}

bool    collaboration::dropARE(const Vector &pos, const string &arm)
{
    Bottle cmd, target, rep;
    bool ret = false;

    cmd.addVocab(Vocab::encode("drop"));
    cmd.addString("over");
    target.addString("cartesian");
    for (int8_t i=0; i<pos.size(); i++)
        target.addDouble(pos[i]);
    cmd.addList() = target;
    cmd.addString(arm.c_str());
    cmd.addString("still");


    yDebug("Command sent to ARE: %s",cmd.toString().c_str());

    if (rpcARE.write(cmd, rep))
        ret = (rep.get(0).asVocab()==Vocab::encode("ack"));

    yDebug() << "[dropARE] Reply from ARE: " << rep.toString();
    return ret;
}

bool    collaboration::checkPosReachable(const Vector &pos, const string &arm)
{
    bool ok = (pos[0] >= workspaceX); //workspaceX is negative
    ok = ok && (pos[2] <= workspaceZ_high) && (pos[2] >= workspaceZ_low);
    if (arm=="left")
    {
        ok = ok && (pos[1] >= -workspaceY) && (pos[1] <=  0.5*workspaceY);
    }
    else if (arm=="right")
    {
        ok = ok && (pos[1] >= -0.5*workspaceY) && (pos[1] <=  workspaceY);
    }
    else
        return false;
    yDebug("targetPos = %s",pos.toString(3,3).c_str());
    yDebug("workspace x= %f, y=%f, z=[%f, %f]", workspaceX, workspaceY, workspaceZ_low, workspaceZ_high);
    return ok;
}

bool    collaboration::lookAtHome(const Vector &ang, const double &timeout)
{
    igaze -> restoreContext(contextGaze);
    igaze -> lookAtAbsAnglesSync(ang);
    igaze -> waitMotionDone(0.1,timeout);
}
