#include "kinematic_maps/SweepFlux.h"

#define XML_CHECK(x) {xmltmp=handle.FirstChildElement(x).ToElement();if (!xmltmp) {INDICATE_FAILURE; return PAR_ERR;}}
#define XML_OK(x) if(!ok(x)){INDICATE_FAILURE; return PAR_ERR;}

//#define DEBUG_MODE
REGISTER_TASKMAP_TYPE("SweepFlux", exotica::SweepFlux);
namespace exotica
{

    SweepFlux::SweepFlux() : initialised_(false), T_(0), init_int_(false), init_vis_(false)
    {

    }

    EReturn SweepFlux::setTimeSteps(const int T)
    {
        int n,m;
        n=scene_->getNumJoints();
        taskSpaceDim(m);
        T_=T;
        J_.resize(m,n); J_.setZero();
        y_.resize(m); y_.setZero();
        Verts_=Eigen::VectorXd::Zero(scene_->getMapSize()*T_*3);

        if(T_<2)
        {
            INDICATE_FAILURE; return FAILURE;
        }
        if(scene_->getMapSize()<2)
        {
            INDICATE_FAILURE; return FAILURE;
        }
        TrisStride_=(scene_->getMapSize()-1)*2+(capTop_->data?1:0)+(capBottom_->data?1:0);
        if(capEnds_->data)
        {
            Tris_.resize(TrisStride_*T_*3);
            TriFlux_.resize(TrisStride_*T_);
            FluxQ_.resize(TrisQ_.rows()/3*T_);
        }
        else
        {
            Tris_.resize(TrisStride_*(T_-1)*3);
            TriFlux_.resize(TrisStride_*(T_-1));
            FluxQ_.resize(TrisQ_.rows()/3*(T_));
        }
        FluxQ_.setZero();
        int a, b, c, d;
        for(int t=0;t<(capEnds_->data?T_:(T_-1));t++)
        {
            if(capTop_->data)
            {
                a=t*(scene_->getMapSize());
                b=((t+1)%T_)*(scene_->getMapSize());
                c=0;
                Tris_(t*TrisStride_*3)=a;
                Tris_(t*TrisStride_*3+1)=b;
                Tris_(t*TrisStride_*3+2)=c;
            }
            if(capBottom_->data)
            {
                b=t*(scene_->getMapSize())+scene_->getMapSize()-1;
                a=((t+1)%T_)*(scene_->getMapSize())+scene_->getMapSize()-1;
                c=scene_->getMapSize()-1;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+(scene_->getMapSize()-1)*2)*3)=a;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+(scene_->getMapSize()-1)*2)*3+1)=b;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+(scene_->getMapSize()-1)*2)*3+2)=c;
            }
            for(int i=0;i<scene_->getMapSize()-1;i++)
            {
                a=t*(scene_->getMapSize())+i;
                b=t*(scene_->getMapSize())+i+1;
                c=((t+1)%T_)*(scene_->getMapSize())+i;
                d=((t+1)%T_)*(scene_->getMapSize())+i+1;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+i*2+0)*3)=a;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+i*2+0)*3+1)=b;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+i*2+0)*3+2)=c;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+i*2+1)*3)=c;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+i*2+1)*3+1)=b;
                Tris_(((capTop_->data?1:0)+t*TrisStride_+i*2+1)*3+2)=d;
            }
        }
        //ROS_WARN_STREAM("\n"<<Tris_);
        TriFlux_.setZero();
        initialised_=true;
        return SUCCESS;

    }

    SweepFlux::~SweepFlux()
    {

    }

    void SweepFlux::initVis(ros::NodeHandle nh ,std::string topic)
    {
        nh_=nh;
        vis_pub_=nh_.advertise<visualization_msgs::Marker>( topic, 0 );
        init_vis_=true;
    }

    void SweepFlux::doVis()
    {

        visualization_msgs::Marker marker;
        marker.header.frame_id = scene_->getRootName();
        marker.header.stamp = ros::Time();
        marker.id = 0;
        marker.ns = "SweepFlux - "+getObjectName();
        marker.type = visualization_msgs::Marker::TRIANGLE_LIST;
        marker.action = visualization_msgs::Marker::ADD;
        marker.pose.position.x = 0;
        marker.pose.position.y = 0;
        marker.pose.position.z = 0;
        marker.pose.orientation.x = 0.0;
        marker.pose.orientation.y = 0.0;
        marker.pose.orientation.z = 0.0;
        marker.pose.orientation.w = 1.0;
        marker.scale.x = 1;
        marker.scale.y = 1;
        marker.scale.z = 1;
        marker.color.a = 1.0;
        marker.color.r = 1.0;
        marker.color.g = 1.0;
        marker.color.b = 1.0;
        marker.points.resize(TrisQ_.rows()+Tris_.rows()*2);
        marker.colors.resize(TrisQ_.rows()+Tris_.rows()*2);
        //ROS_WARN_STREAM(TrisQ_.rows()<<"x"<<TrisQ_.cols()<<"\n"<<VertsQ_.rows()<<"x"<<VertsQ_.cols());
        double fmax=-1e100,fmin=1e100;
        for(int i=0;i<TrisQ_.rows()/3;i++)
        {
            Eigen::Vector3d tmp1=VertsQ_.middleRows(TrisQ_(i*3)*3,3);
            Eigen::Vector3d tmp2=VertsQ_.middleRows(TrisQ_(i*3+1)*3,3);
            Eigen::Vector3d tmp3=VertsQ_.middleRows(TrisQ_(i*3+2)*3,3);
            Eigen::Vector3d tmp=(tmp2-tmp1).cross(tmp3-tmp1);
            Eigen::Vector3d zz;
            zz << 0.0, 0.0, 1.0;
            tmp=tmp/tmp.norm();
            if(tmp(0)!=tmp(0))
            {
                tmp.setZero();
            }
            else
            {
                tmp(0)=tmp.dot(zz);
            }
            double flx=0.0;
            for(int j=0;j<T_;j++)
            {
                flx+=FluxQ_[i+j*(TrisQ_.rows()/3)];
            }
            fmin=std::min(fmin,flx);
            fmax=std::max(fmax,flx);
            for(int j=0;j<3;j++)
            {
                marker.points[i*3+j].x=VertsQ_(TrisQ_(i*3+j)*3);
                marker.points[i*3+j].y=VertsQ_(TrisQ_(i*3+j)*3+1);
                marker.points[i*3+j].z=VertsQ_(TrisQ_(i*3+j)*3+2);
                marker.colors[i*3+j].r=flx;
                marker.colors[i*3+j].g=0.0;
                marker.colors[i*3+j].b=0.0;
                marker.colors[i*3+j].a=1.0;
            }
        }
        if(fabs(fmin)<fabs(fmax))
        {
            fmin=fmin/fabs(fmin)*fabs(fmax);
        }
        else
        {
            fmax=fmax/fabs(fmax)*fabs(fmin);
        }
        for(int i=0;i<TrisQ_.rows()/3;i++)
        {
            for(int j=0;j<3;j++)
            {
                double flx=marker.colors[i*3+j].r;
                marker.colors[i*3+j].r=0.0;
                if(flx>0)
                {
                    marker.colors[i*3+j].r=flx/fmax;
                }
                if(flx<0)
                {
                    marker.colors[i*3+j].b=flx/fmin;
                }
            }
        }
        for(int i=0;i<Tris_.rows()/3;i++)
        {
            for(int j=0;j<3;j++)
            {
                marker.points[TrisQ_.rows()+i*3+j].x=Verts_(Tris_(i*3+j)*3);
                marker.points[TrisQ_.rows()+i*3+j].y=Verts_(Tris_(i*3+j)*3+1);
                marker.points[TrisQ_.rows()+i*3+j].z=Verts_(Tris_(i*3+j)*3+2);
                marker.colors[TrisQ_.rows()+i*3+j].r=0.2;
                marker.colors[TrisQ_.rows()+i*3+j].g=1.0-((i/TrisStride_)%2==0?0.0:0.3);
                marker.colors[TrisQ_.rows()+i*3+j].b=0.2;
                marker.colors[TrisQ_.rows()+i*3+j].a=1.0;
            }
        }
        for(int i=0;i<Tris_.rows()/3;i++)
        {
            Eigen::Vector3d tmp1=Verts_.middleRows(Tris_(i*3)*3,3);
            Eigen::Vector3d tmp2=Verts_.middleRows(Tris_(i*3+1)*3,3);
            Eigen::Vector3d tmp3=Verts_.middleRows(Tris_(i*3+2)*3,3);
            Eigen::Vector3d tmp=(tmp2-tmp1).cross(tmp3-tmp1);
            tmp=tmp/tmp.norm()*0.001;
            if(tmp(0)!=tmp(0))
            {
                tmp.setZero();
            }
            for(int j=0;j<3;j++)
            {
                marker.points[TrisQ_.rows()+i*3+j+Tris_.rows()].x=Verts_(Tris_(i*3+j)*3)-tmp(0);
                marker.points[TrisQ_.rows()+i*3+j+Tris_.rows()].y=Verts_(Tris_(i*3+j)*3+1)-tmp(1);
                marker.points[TrisQ_.rows()+i*3+j+Tris_.rows()].z=Verts_(Tris_(i*3+j)*3+2)-tmp(2);
                marker.colors[TrisQ_.rows()+i*3+j+Tris_.rows()].r=0.2;
                marker.colors[TrisQ_.rows()+i*3+j+Tris_.rows()].g=0.2;
                marker.colors[TrisQ_.rows()+i*3+j+Tris_.rows()].b=1.0-((i/TrisStride_)%2==0?0.0:0.3);
                marker.colors[TrisQ_.rows()+i*3+j+Tris_.rows()].a=1.0;
            }
        }
        vis_pub_.publish( marker );

    }

    EReturn SweepFlux::update(const Eigen::VectorXd & x, const int t)
    {
        LOCK(scene_lock_);
        invalidate();
        if (scene_ == nullptr)
        {
            INDICATE_FAILURE
            ;
            return MMB_NIN;
        }
        if (!initialised_)
        {
            INDICATE_FAILURE
            ;
            return MMB_NIN;
        }
        if (ok(computeJacobian(x,t)))
        {
            setPhi(y_,t);
            setJacobian(J_,t);
        }
        else
        {
            INDICATE_FAILURE
            ;
            return FAILURE;
        }

        //!< Phi will be updated in computeJacobian
        return SUCCESS;
    }

    EReturn SweepFlux::taskSpaceDim(int & task_dim)
    {
        task_dim = 1;
        return SUCCESS;
    }

    EReturn SweepFlux::initDerived(tinyxml2::XMLHandle & handle)
    {
        tinyxml2::XMLHandle tmp_handle = handle.FirstChildElement("ObjectMesh");
        server_->registerRosParam<std::string>(ns_, tmp_handle, obj_file_);
        if(!ok(loadOBJ(*obj_file_,TrisQ_,VertsQ_orig_)))
        {
            return PAR_ERR;
        }

        tmp_handle = handle.FirstChildElement("CapTop");
        server_->registerParam<std_msgs::Bool>(ns_, tmp_handle, capTop_);
        tmp_handle = handle.FirstChildElement("CapBottom");
        server_->registerParam<std_msgs::Bool>(ns_, tmp_handle, capBottom_);
        tmp_handle = handle.FirstChildElement("CapEnds");
        server_->registerParam<std_msgs::Bool>(ns_, tmp_handle, capEnds_);
        //tmp_handle = handle.FirstChildElement("ObjectPose");
        //server_->registerParam<geometry_msgs::PoseStamped>(ns_, tmp_handle, obj_pose_);
        Eigen::Affine3d val;
        transform(val);

        init_int_=true;
        return SUCCESS;
    }

    void SweepFlux::transform(Eigen::Affine3d& val)
    {
        qTransform_=val;
        VertsQ_.resize(VertsQ_orig_.rows());
        for(int i=0;i<VertsQ_.rows()/3;i++)
        {
            VertsQ_.segment(i*3,3)=qTransform_*((Eigen::Vector3d)VertsQ_orig_.segment(i*3,3));
        }
    }

    EReturn SweepFlux::computeJacobian(const Eigen::VectorXd & q, const int t)
    {
        std::vector<std::string> temp_vector;
        Eigen::VectorXd tmp(scene_->getMapSize()*3);
        bool success = scene_->getForwardMap(tmp, temp_vector);
        if(!success) { INDICATE_FAILURE; return FAILURE; }
        Verts_.segment(t*scene_->getMapSize()*3,scene_->getMapSize()*3)=tmp;
        //ROS_WARN_STREAM("ID range: "<<t*scene_->getMapSize()<<" to "<<t*scene_->getMapSize()+scene_->getMapSize()-1);
        //ROS_WARN_STREAM("\n"<<Verts_.block(t*scene_->getMapSize()*3,0,scene_->getMapSize()*3,1)<<"\n---\n"<<tmp);
        VertJ_= Eigen::MatrixXd::Zero(scene_->getMapSize()*3*3,q.rows());
        Eigen::MatrixXd tmpM(scene_->getMapSize()*3,q.rows());
        success = scene_->getJacobian(tmpM);
        if(!success) { INDICATE_FAILURE; return FAILURE; }
        if(t==0)
        {
            VertJ_.block(0,0,scene_->getMapSize()*3,q.rows())=tmpM;
        }
        else
        {
            VertJ_.block(scene_->getMapSize()*3,0,scene_->getMapSize()*3,q.rows())=tmpM;
        }

        int tri_start=(t+(t==0?0:-1))*TrisStride_;
        //int tri_start=(t+((t==T_-1&&!capEnds_->data)?-1:0))*TrisStride_;
        int tri_length=((t==-1||((t==T_-1)&&capEnds_->data))?2:1)*TrisStride_;
       /* FluxTriangleTriangle(
                    Tris_.segment
                    (tri_start*3,
                     tri_length*3),
                    Verts_,
                    VertJ_,
                    Tris_.segment(0,TrisStride_*3*3),
                    TrisQ_,VertsQ_,TriFlux_.segment(tri_start,tri_length),J_);*/

        FluxTriangleTriangle(
                    &Tris_.data()[tri_start*3],
                    Verts_.data(),
                    VertJ_.data(),
                    Tris_.data(),
                    TrisQ_.data(),
                    VertsQ_.data(),
                    &TriFlux_.data()[tri_start],
                    J_.data(),
                    tri_length,
                    VertJ_.rows(),
                    VertJ_.cols(),
                    TrisQ_.rows()/3,
                    &FluxQ_.data()[(t+(t==0?0:-1))*(TrisQ_.rows()/3)]);

        //y_(0)=TriFlux_.sum();///((double)T_);

        //J_=J_/((double)T_);
        y_(0)=TriFlux_.segment(tri_start,tri_length).sum();
/*
        if(t==T_-1)
        {
            //ROS_WARN_STREAM("\n"<<TriFlux_.transpose());
            //ROS_WARN_STREAM(J_);
            ROS_WARN_STREAM(TriFlux_.sum());
        }
*/
        return SUCCESS;
    }

    void SweepFlux::FluxTriangleTriangle(int* Tris,
                              double* Verts,
                              double* VertJ,
                              int* TrisJ,
                              int* TrisQ,
                              double* VertsQ,
                              double* Flux,
                              double* FluxJ,int nTris,
                              int nTrisJ,int n,int nTrisQ,
                              double* FluxQ)
    {
        memset(Flux,0,sizeof(double)*nTris);
        memset(FluxJ,0,sizeof(double)*n);
        double *a,*b,*c,*d,*e,*f,tmp[3],m1[3],m2[3],m3[3],m4[3],*aJ,*bJ,*cJ,*tmpFluxJ,area,f0,f1,f2,f3;
        aJ=new double[3*n];
        bJ=new double[3*n];
        cJ=new double[3*n];
        tmpFluxJ=new double[n*4];

        for(int j=0;j<nTrisQ;j++)
        {
            FluxQ[j]=0.0;
            d=&VertsQ[TrisQ[j*3]*3];
            e=&VertsQ[TrisQ[j*3+1]*3];
            f=&VertsQ[TrisQ[j*3+2]*3];
            cross(e,d,f,d,tmp);
            area=norm(tmp)*0.5;
            for(int i=0;i<nTris;i++)
            {
                a=&Verts[Tris[i*3]*3];
                b=&Verts[Tris[i*3+1]*3];
                c=&Verts[Tris[i*3+2]*3];

                for(int k=0;k<n;k++)
                {
                    aJ[k*3]=VertJ[TrisJ[i*3]*3+k*nTrisJ];
                    aJ[k*3+1]=VertJ[TrisJ[i*3]*3+k*nTrisJ+1];
                    aJ[k*3+2]=VertJ[TrisJ[i*3]*3+k*nTrisJ+2];

                    bJ[k*3]=VertJ[TrisJ[i*3+1]*3+k*nTrisJ];
                    bJ[k*3+1]=VertJ[TrisJ[i*3+1]*3+k*nTrisJ+1];
                    bJ[k*3+2]=VertJ[TrisJ[i*3+1]*3+k*nTrisJ+2];

                    cJ[k*3]=VertJ[TrisJ[i*3+2]*3+k*nTrisJ];
                    cJ[k*3+1]=VertJ[TrisJ[i*3+2]*3+k*nTrisJ+1];
                    cJ[k*3+2]=VertJ[TrisJ[i*3+2]*3+k*nTrisJ+2];
                }

                for(int k=0;k<3;k++)
                {

                    m1[k]=(4.0*d[k]+e[k]+f[k])/6.0;
                    m2[k]=(d[k]+4.0*e[k]+f[k])/6.0;
                    m3[k]=(d[k]+e[k]+4.0*f[k])/6.0;
                    m4[k]=(d[k]+e[k]+f[k])/3.0;
                }

                FluxPointTriangle(m1,a,b,c,aJ,bJ,cJ,&f0,&tmpFluxJ[0],n);
                FluxPointTriangle(m2,a,b,c,aJ,bJ,cJ,&f1,&tmpFluxJ[n],n);
                FluxPointTriangle(m3,a,b,c,aJ,bJ,cJ,&f2,&tmpFluxJ[n*2],n);
                FluxPointTriangle(m4,a,b,c,aJ,bJ,cJ,&f3,&tmpFluxJ[n*3],n);
                Flux[i]+=(f0+f1+f2+f3)*area;
                FluxQ[j]+=(f0+f1+f2+f3)*area;

                for(int k=0;k<n;k++)
                  FluxJ[k]+=(tmpFluxJ[k]+tmpFluxJ[k+n]+tmpFluxJ[k+2*n]+tmpFluxJ[k+3*n])*area;
            }
        }

        delete[] aJ;
        delete[] bJ;
        delete[] cJ;
        delete[] tmpFluxJ;
    }

    void SweepFlux::FluxTriangleTriangle(const Eigen::Ref<const Eigen::VectorXi> & Tris,
                              const Eigen::Ref<const Eigen::VectorXd> & Verts,
                              const Eigen::Ref<const Eigen::MatrixXd> & VertJ,
                              const Eigen::Ref<const Eigen::VectorXi> & TrisJ,
                              const Eigen::Ref<const Eigen::VectorXi> & TrisQ,
                              const Eigen::Ref<const Eigen::VectorXd> & VertsQ,
                              Eigen::Ref<Eigen::VectorXd> Flux,
                              Eigen::Ref<Eigen::MatrixXd> FluxJ)
    {
        //Eigen::VectorXd tFlux(Tris.rows());
        //Eigen::MatrixXd tFluxJ(Tris.rows(),VertJ.cols());
        Flux.setZero();
        FluxJ.setZero();
        int i,j;
        Eigen::Vector3d a,b,c,d,e,f,tmp,m1,m2,m3,m4;
        Eigen::MatrixXd aJ,bJ,cJ;
        //ROS_ERROR_STREAM(Tris.minCoeff()<<" "<<Tris.maxCoeff());
        //ROS_ERROR_STREAM(TrisJ);
        Eigen::MatrixXd tmpFluxJ(4,VertJ.cols());
        double area, thr=1.0/3.0,f0,f1,f2,f3;
        for(j=0;j<TrisQ.rows()/3;j++)
        {
            d=VertsQ.segment(TrisQ(j*3)*3,3);
            e=VertsQ.segment(TrisQ(j*3+1)*3,3);
            f=VertsQ.segment(TrisQ(j*3+2)*3,3);
            tmp=(e-d).cross(f-d);
            area = tmp.norm()*0.5;
            for(i=0;i<Tris.rows()/3;i++)
            {
                a=Verts.segment(Tris(i*3)*3,3);
                b=Verts.segment(Tris(i*3+1)*3,3);
                c=Verts.segment(Tris(i*3+2)*3,3);
                aJ=VertJ.middleRows(TrisJ(i*3)*3,3);
                bJ=VertJ.middleRows(TrisJ(i*3+1)*3,3);
                cJ=VertJ.middleRows(TrisJ(i*3+2)*3,3);

                /*FluxPointTriangle(Eigen::Vector3d((4.0*d+e+f)*thr*thr),a,b,c,aJ,bJ,cJ,f0,tmpFluxJ.block(0,0,1,tmpFluxJ.cols()));
                FluxPointTriangle(Eigen::Vector3d((d+4.0*e+f)*thr*thr),a,b,c,aJ,bJ,cJ,f1,tmpFluxJ.block(1,0,1,tmpFluxJ.cols()));
                FluxPointTriangle(Eigen::Vector3d((d+e+4.0*f)*thr*thr),a,b,c,aJ,bJ,cJ,f2,tmpFluxJ.block(2,0,1,tmpFluxJ.cols()));
                FluxPointTriangle(Eigen::Vector3d((d+e+f)*thr        ),a,b,c,aJ,bJ,cJ,f3,tmpFluxJ.block(3,0,1,tmpFluxJ.cols()));*/
                //if(aJ(0,0)!=0)
                {
                    //ROS_ERROR_STREAM(aJ);
                    //ROS_ERROR_STREAM(aJ.data()[0]<<" "<<aJ.data()[1]<<" "<<aJ.data()[2]<<" "<<aJ.data()[3]<<" "<<aJ.data()[4]<<" "<<aJ.data()[5]<<" "<<aJ.data()[6]);
                }
                m1=(4.0*d+e+f)*thr*thr;
                m2=(d+4.0*e+f)*thr*thr;
                m3=(d+e+4.0*f)*thr*thr;
                m4=(d+e+f)*thr;

                //ROS_ERROR_STREAM(aJ);
                FluxPointTriangle(m1.data(),a.data(),b.data(),c.data(),aJ.data(),bJ.data(),cJ.data(),&f0,tmpFluxJ.block(0,0,1,tmpFluxJ.cols()).data(),tmpFluxJ.cols());
                //ROS_ERROR_STREAM(aJ);
                FluxPointTriangle(m2.data(),a.data(),b.data(),c.data(),aJ.data(),bJ.data(),cJ.data(),&f1,tmpFluxJ.block(1,0,1,tmpFluxJ.cols()).data(),tmpFluxJ.cols());
                //ROS_ERROR_STREAM(aJ);
                FluxPointTriangle(m3.data(),a.data(),b.data(),c.data(),aJ.data(),bJ.data(),cJ.data(),&f2,tmpFluxJ.block(2,0,1,tmpFluxJ.cols()).data(),tmpFluxJ.cols());
                //ROS_ERROR_STREAM(aJ);
                FluxPointTriangle(m4.data(),a.data(),b.data(),c.data(),aJ.data(),bJ.data(),cJ.data(),&f3,tmpFluxJ.block(3,0,1,tmpFluxJ.cols()).data(),tmpFluxJ.cols());

                //ROS_ERROR_STREAM(f0<<" "<<f1<<" "<<f2<<" "<<f3<<" "<<area);
                Flux(i)+=(f0+f1+f2+f3)*area;
                for(int k=0;k<FluxJ.cols();k++)
                    FluxJ(0,k)+=(tmpFluxJ(0,k)+tmpFluxJ(1,k)+tmpFluxJ(2,k)+tmpFluxJ(3,k))*area;
            }
        }
        //ROS_WARN_STREAM(FluxJ);
        //Flux+=tFlux;
        //FluxJ+=tFluxJ;
    }

    void SweepFlux::FluxPointTriangle(const Eigen::Ref<const Eigen::Vector3d> & x,
                           const Eigen::Ref<const Eigen::Vector3d> & a,
                           const Eigen::Ref<const Eigen::Vector3d> & b,
                           const Eigen::Ref<const Eigen::Vector3d> & c,
                           const Eigen::Ref<const Eigen::MatrixXd> & aJ,
                           const Eigen::Ref<const Eigen::MatrixXd> & bJ,
                           const Eigen::Ref<const Eigen::MatrixXd> & cJ,
                           double& Flux,
                           Eigen::Ref<Eigen::MatrixXd> FluxJ)
    {
        double J,K,_J,_K,nax,nbx,ncx,_nax,_nbx,_ncx,dab,dac,dcb;
        Eigen::Vector3d tmp, tmp1,tmp2, _a,_b,_c;
        nax=(x-a).norm();
        nbx=(x-b).norm();
        ncx=(x-c).norm();
        dab=(x-a).dot(x-b);
        dac=(x-a).dot(x-c);
        dcb=(x-c).dot(x-b);

        tmp=(x-a).cross(x-b);
        J=tmp.dot(x-c);
        K=nax*nbx*ncx+dab*ncx+dac*nbx+dcb*nax;
        Flux=0;
        FluxJ.setZero();
        if(K*K<1e-50 || nax<1e-50 || nbx<1e-50 || ncx<1e-50) return;
        Flux = 2.0*atan2(J,K);


        int j;

        for (j=0;j<FluxJ.cols();j++)
        {
            _a(0)=-aJ(0,j);_a(1)=-aJ(1,j);_a(2)=-aJ(2,j);
            _b(0)=-bJ(0,j);_b(1)=-bJ(1,j);_b(2)=-bJ(2,j);
            _c(0)=-cJ(0,j);_c(1)=-cJ(1,j);_c(2)=-cJ(2,j);
            _nax=_a.dot(x-a)/nax;
            _nbx=_b.dot(x-b)/nbx;
            _ncx=_c.dot(x-c)/ncx;
            tmp1=_a.cross(x-b);
            tmp2=(x-a).cross(_b);
            _J=(tmp1+tmp2).dot(x-c)+tmp.dot(_c);
            _K=_nax*nbx*ncx+nax*_nbx*ncx+nax*nbx*_ncx
                    +(_a.dot(x-b)+(x-a).dot(_b))*ncx+dab*_ncx
                    +(_a.dot(x-c)+(x-a).dot(_c))*nbx+dac*_nbx
                    +(_c.dot(x-b)+(x-c).dot(_b))*nax+dcb*_nax;

            if((J*J+K*K)<1e-50) continue;

            FluxJ(0,j)=2.0*(_J*K-J*_K)/(J*J+K*K);
        }
    }

}
